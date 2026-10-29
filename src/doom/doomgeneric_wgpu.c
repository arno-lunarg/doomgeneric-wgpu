// doomgeneric platform backend: Win32 window, keyboard, timer.
// Frames go through WebGPU (tutorial step 2: tutorial/step2.html). -gdi keeps the GDI blit as a pixel reference.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "doomgeneric.h"
#include "doomkeys.h"
#include "m_argv.h"
#include "i_system.h"

#include "gpu.h"
#include "platform_win32.h"

// Doom ran 320x200 on 4:3 CRTs: pixels are 20% taller than wide.
#define DISPLAY_ASPECT (4.0f / 3.0f)

#define KEYQUEUE_SIZE 16

static unsigned short s_keyQueue[KEYQUEUE_SIZE];
static unsigned int s_keyQueueWrite;
static unsigned int s_keyQueueRead;

static LARGE_INTEGER s_timerFrequency;
static LARGE_INTEGER s_timerStart;

static bool s_useGdi;  // -gdi: CPU blit, reference image for the GPU path
static Gpu s_gpu;
static WGPUTexture s_frame_texture;
static WGPUTextureView s_frame_texture_view;
static WGPURenderPipeline s_render_pipeline;

// TODO(§2, §4) Frame texture, view, sampler, bind group, pipeline.

static unsigned char ConvertToDoomKey(unsigned int vk)
{
    if (vk >= VK_F1 && vk <= VK_F10)
        return (unsigned char)(KEY_F1 + (vk - VK_F1));

    switch (vk)
    {
        case VK_RETURN:    return KEY_ENTER;
        case VK_ESCAPE:    return KEY_ESCAPE;
        case VK_TAB:       return KEY_TAB;
        case VK_BACK:      return KEY_BACKSPACE;
        case VK_PAUSE:     return KEY_PAUSE;
        case VK_LEFT:      return KEY_LEFTARROW;
        case VK_RIGHT:     return KEY_RIGHTARROW;
        case VK_UP:        return KEY_UPARROW;
        case VK_DOWN:      return KEY_DOWNARROW;
        case VK_CONTROL:   return KEY_FIRE;
        case VK_SPACE:     return KEY_USE;
        case VK_SHIFT:     return KEY_RSHIFT;
        case VK_MENU:      return KEY_RALT;
        case VK_F11:       return KEY_F11;
        case VK_F12:       return KEY_F12;
        case VK_OEM_MINUS: return KEY_MINUS;
        case VK_OEM_PLUS:  return KEY_EQUALS;
        default:           return (unsigned char)tolower((int)vk);
    }
}

static void OnKey(bool pressed, unsigned int vk)
{
    s_keyQueue[s_keyQueueWrite] = (unsigned short)((pressed << 8) | ConvertToDoomKey(vk));
    s_keyQueueWrite = (s_keyQueueWrite + 1) % KEYQUEUE_SIZE;
}

// Largest DISPLAY_ASPECT rect centered in the client area. GDI blit target, GPU viewport.
// e.g. 1280x720 client -> x=160 y=0 w=960 h=720.
static void LetterboxRect(int* x, int* y, int* w, int* h)
{
    int clientW = (int)g_platform.width;
    int clientH = (int)g_platform.height;

    *w = clientW;
    *h = (int)(clientW / DISPLAY_ASPECT);
    if (*h > clientH)
    {
        *h = clientH;
        *w = (int)(clientH * DISPLAY_ASPECT);
    }
    *x = (clientW - *w) / 2;
    *y = (clientH - *h) / 2;
}

// Black bars around the letterbox rect.
static void BlitGdi(void)
{
    HWND hwnd = g_platform.hwnd;
    HDC hdc = GetDC(hwnd);

    int clientW = (int)g_platform.width;
    int clientH = (int)g_platform.height;
    int x, y, w, h;
    LetterboxRect(&x, &y, &w, &h);

    PatBlt(hdc, 0, 0, x, clientH, BLACKNESS);
    PatBlt(hdc, x + w, 0, clientW - x - w, clientH, BLACKNESS);
    PatBlt(hdc, x, 0, w, y, BLACKNESS);
    PatBlt(hdc, x, y + h, w, clientH - y - h, BLACKNESS);

    // DG_ScreenBuffer is 0x00RRGGBB: BGRA bytes in memory, which is what a 32-bit DIB expects.
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = DOOMGENERIC_RESX;
    bmi.bmiHeader.biHeight = -DOOMGENERIC_RESY;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(hdc, x, y, w, h, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, DG_ScreenBuffer, &bmi,
                  DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(hwnd, hdc);
}

// -vulkan / -d3d12 / -d3d11 pick the Dawn backend. None: Dawn's choice (D3D12).
static WGPUBackendType ParseBackend(void)
{
    return WGPUBackendType_Vulkan;
}

static void CreateFrameResources(void)
{
    // TODO(§2) Frame texture and its view.
    WGPUTextureDescriptor texture_desc = WGPU_TEXTURE_DESCRIPTOR_INIT;
    texture_desc.label = Gpu_Str("Doom blit texture");
    texture_desc.usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
    texture_desc.dimension = WGPUTextureDimension_2D;
    texture_desc.size = (WGPUExtent3D) { .width = DOOMGENERIC_RESX, .height = DOOMGENERIC_RESY, .depthOrArrayLayers = 1 };
    texture_desc.format = WGPUTextureFormat_BGRA8Unorm;
    texture_desc.mipLevelCount = 1;
    texture_desc.sampleCount = 1;

    s_frame_texture = wgpuDeviceCreateTexture(s_gpu.device, &texture_desc);
    s_frame_texture_view = wgpuTextureCreateView(s_frame_texture, NULL);
    // TODO(§4) Sampler, bind group layout, pipeline layout, pipeline, bind group.
}

// Returns false while unimplemented: DG_DrawFrame then falls back to GDI.
static bool DrawGpu(void)
{
    // TODO(§1) Gpu_BeginFrame, clear pass, Gpu_EndFrame.
    WGPUTextureView target = Gpu_BeginFrame(&s_gpu);
    if (!target) {
        return true;
    }

    WGPUTexelCopyTextureInfo dest_tex = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    dest_tex.texture = s_frame_texture;
    dest_tex.mipLevel = 0;
    dest_tex.origin = (WGPUOrigin3D) { 0, 0, 0 };
    dest_tex.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout src_buffer_layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    src_buffer_layout.offset = 0;
    src_buffer_layout.bytesPerRow = DOOMGENERIC_RESX * sizeof(DG_ScreenBuffer[0]);
    src_buffer_layout.rowsPerImage = DOOMGENERIC_RESY;

    const uint32_t cpu_screen_buffer_size = DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(DG_ScreenBuffer[0]);
    WGPUExtent3D write_extent = { DOOMGENERIC_RESX, DOOMGENERIC_RESY, 1 };
    wgpuQueueWriteTexture(s_gpu.queue, &dest_tex, DG_ScreenBuffer, cpu_screen_buffer_size, &src_buffer_layout, &write_extent);

    WGPUCommandEncoderDescriptor cmd_encoder_desc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder cmd_encoder = wgpuDeviceCreateCommandEncoder(s_gpu.device, &cmd_encoder_desc);
    //assert(cmd_encoder);

    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = target;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = (WGPUColor) { 0.2, 0.1, 0.12, 1.0 };

    WGPURenderPassDescriptor passDesc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    passDesc.label = Gpu_Str("Triangle pass");
    passDesc.colorAttachmentCount = 1;
    passDesc.colorAttachments = &color;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(cmd_encoder, &passDesc);

    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBufferDescriptor cb_desc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer cb = wgpuCommandEncoderFinish(cmd_encoder, &cb_desc);

    wgpuQueueSubmit(s_gpu.queue, 1, &cb);
    
    wgpuCommandBufferRelease(cb);
    wgpuCommandEncoderRelease(cmd_encoder);

    Gpu_EndFrame(&s_gpu, target);
    // TODO(§3) Upload DG_ScreenBuffer.
    

    // TODO(§5) Viewport, pipeline, bind group, draw.
    return true;
}

// Registered with atexit: Doom leaves through exit() from several places.
static void ShutdownGpu(void)
{
    // TODO(§1, §5) Release what CreateFrameResources made, then Gpu_Shutdown.
    wgpuTextureRelease(s_frame_texture);
    Gpu_Shutdown(&s_gpu);
}

void DG_Init(void)
{
    // 3x integer scale of the 4:3 display size (320x240).
    if (!Platform_CreateWindow("Doom", 960, 720))
    {
        fprintf(stderr, "Window creation failed\n");
        exit(1);
    }
    g_platform.onKey = OnKey;

    QueryPerformanceFrequency(&s_timerFrequency);
    QueryPerformanceCounter(&s_timerStart);

    s_useGdi = M_CheckParm("-gdi") > 0;
    if (s_useGdi)
        return;

    // TODO(§1) Gpu_Init on g_platform's window with ParseBackend(), atexit(ShutdownGpu).
    Gpu_Init(&s_gpu, g_platform.hinstance, g_platform.hwnd, g_platform.width, g_platform.height, ParseBackend());

    CreateFrameResources();
    I_AtExit(ShutdownGpu, true);
}

void DG_DrawFrame(void)
{
    if (!Platform_PumpMessages())
        exit(0);

    if (g_platform.width == 0 || g_platform.height == 0)
        return;

    // TODO(§1) Gpu_Resize when g_platform.resized.
    if (g_platform.resized) {
        Gpu_Resize(&s_gpu, g_platform.width, g_platform.height);
        g_platform.resized = false;
    }

    if (s_useGdi || !DrawGpu())
        BlitGdi();
}

void DG_SleepMs(uint32_t ms)
{
    Sleep(ms);
}

// QueryPerformanceCounter, not GetTickCount: 15.6 ms granularity stutters at Doom's 35 Hz tic rate.
uint32_t DG_GetTicksMs(void)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (uint32_t)((now.QuadPart - s_timerStart.QuadPart) * 1000 / s_timerFrequency.QuadPart);
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    if (s_keyQueueRead == s_keyQueueWrite)
        return 0;

    unsigned short keyData = s_keyQueue[s_keyQueueRead];
    s_keyQueueRead = (s_keyQueueRead + 1) % KEYQUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;
    return 1;
}

void DG_SetWindowTitle(const char* title)
{
    Platform_SetTitle(title);
}

int main(int argc, char** argv)
{
    doomgeneric_Create(argc, argv);

    for (;;)
        doomgeneric_Tick();
}
