// Step 1: device, surface, one triangle. Tutorial: tutorial/step1.html
// The window loop is provided; the TODOs are yours.

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gpu.h"
#include "platform_win32.h"

// TODO(§6) WGSL source: vertex + fragment entry points.

static WGPURenderPipeline CreateTrianglePipeline(Gpu* gpu)
{
    // TODO(§6) Shader module, then render pipeline targeting gpu->surfaceFormat.
    (void)gpu;
    return NULL;
}

static void DrawFrame(Gpu* gpu, WGPURenderPipeline pipeline, WGPUTextureView target)
{
    // TODO(§5) Command encoder, render pass clearing `target`, submit.
    WGPUCommandEncoderDescriptor cmd_encoder_desc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder cmd_encoder = wgpuDeviceCreateCommandEncoder(gpu->device, &cmd_encoder_desc);
    assert(cmd_encoder);

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

    wgpuRenderPassEncoderSetPipeline(pass, gpu->render_pipeline);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);

    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBufferDescriptor cb_desc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer cb = wgpuCommandEncoderFinish(cmd_encoder, &cb_desc);

    wgpuQueueSubmit(gpu->queue, 1, &cb);
    
    wgpuCommandBufferRelease(cb);
    wgpuCommandEncoderRelease(cmd_encoder);
    // TODO(§6) Set `pipeline`, draw 3 vertices.
    (void)gpu, (void)pipeline, (void)target;
}

// -vulkan / -d3d12 / -d3d11 on the command line picks the Dawn backend.
static WGPUBackendType ParseBackend(int argc, char** argv)
{
    (void)argc, (void)argv;
    return WGPUBackendType_Vulkan;
}

int main(int argc, char** argv)
{
    if (!Platform_CreateWindow("Step 1 - Triangle", 1280, 720))
        return 1;

    Gpu gpu = { 0 };
    if (!Gpu_Init(&gpu, g_platform.hinstance, g_platform.hwnd, g_platform.width, g_platform.height,
                  ParseBackend(argc, argv)))
    {
        fprintf(stderr, "Gpu_Init failed\n");
        return 1;
    }

    WGPURenderPipeline pipeline = CreateTrianglePipeline(&gpu);

    while (Platform_PumpMessages())
    {
        if (g_platform.resized)
        {
            g_platform.resized = false;
            Gpu_Resize(&gpu, g_platform.width, g_platform.height);
        }

        // Minimized: nothing to present to.
        if (g_platform.width == 0 || g_platform.height == 0)
        {
            Sleep(16);
            continue;
        }

        WGPUTextureView target = Gpu_BeginFrame(&gpu);
        if (!target)
            continue;

        DrawFrame(&gpu, pipeline, target);
        Gpu_EndFrame(&gpu, target);
    }

    if (pipeline)
        wgpuRenderPipelineRelease(pipeline);
    Gpu_Shutdown(&gpu);
    return 0;
}
