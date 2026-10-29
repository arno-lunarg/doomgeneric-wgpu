#pragma once

// WebGPU context shared by every step: instance, adapter, device, queue, surface.
// You implement gpu.c in tutorial step 1. Change this interface freely.

#include <stdbool.h>
#include <stdint.h>

#include <webgpu/webgpu.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define ASSERT_STATUS_SUCCESS(status) (assert((status) == WGPUStatus_Success))

typedef struct Gpu
{
    WGPUInstance      instance;
    WGPUAdapter       adapter;
    WGPUDevice        device;
    WGPUQueue         queue;
    WGPUSurface       surface;
    WGPUTextureFormat surfaceFormat;
    uint32_t          width;
    uint32_t          height;
    WGPUTexture       frameTexture;  // swapchain image held between Gpu_BeginFrame and Gpu_EndFrame
} Gpu;

// backend: WGPUBackendType_Undefined lets Dawn choose (D3D12 on Windows).
//          WGPUBackendType_Vulkan to inspect the translated Vulkan calls in RenderDoc.
bool Gpu_Init(Gpu* gpu, void* hinstance, void* hwnd, uint32_t width, uint32_t height, WGPUBackendType backend);

// Reconfigures the surface. Ignores 0x0 (minimized window).
void Gpu_Resize(Gpu* gpu, uint32_t width, uint32_t height);

// Acquires the next swapchain image. Returns NULL when this frame must be skipped.
WGPUTextureView Gpu_BeginFrame(Gpu* gpu);

// Presents the image acquired by Gpu_BeginFrame and releases `view`.
void Gpu_EndFrame(Gpu* gpu, WGPUTextureView view);

void Gpu_Shutdown(Gpu* gpu);

// WebGPU strings are (pointer, length) views. WGPU_STRLEN means "null-terminated".
static inline WGPUStringView Gpu_Str(const char* s)
{
    WGPUStringView view = { s, WGPU_STRLEN };
    return view;
}
