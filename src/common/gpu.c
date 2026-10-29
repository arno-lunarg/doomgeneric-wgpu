// Your WebGPU context. Tutorial step 1: tutorial/step1.html
// Each TODO names the tutorial section that covers it.

#include "gpu.h"

#include "shader.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// printf("%.*s", SV(view)): WGPUStringView is not null-terminated unless length == WGPU_STRLEN.
#define SV(sv) (int)((sv).length == WGPU_STRLEN ? strlen((sv).data) : (sv).length), (sv).data


// TODO(§3, §4): callbacks for RequestAdapter, RequestDevice, device lost, uncaptured errors, logging.

static void on_adapter_get
            (WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2)
{
    (void)message;
    (void)userdata2;
    assert(status == WGPURequestAdapterStatus_Success);
    *(WGPUAdapter*)userdata1 = adapter;
}

static void on_device_create
            (WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2)
{
    assert(status == WGPURequestDeviceStatus_Success);
    (void)message;
    (void)userdata2;
    *(WGPUDevice*)userdata1 = device;
}

static void on_device_lost(WGPUDevice const* device, WGPUDeviceLostReason reason, WGPUStringView message,
                           void* userdata1, void* userdata2)
{
    (void)device, (void)userdata1, (void)userdata2;
    // Destroyed / CallbackCancelled: we released the device ourselves.
    if (reason == WGPUDeviceLostReason_Destroyed || reason == WGPUDeviceLostReason_CallbackCancelled) {
        return;
    }
    fprintf(stderr, "[gpu] Device lost (reason %d): %.*s\n", reason, SV(message));
}

static void on_uncaptured_error(WGPUDevice const* device, WGPUErrorType type, WGPUStringView message,
                                void* userdata1, void* userdata2)
{
    (void)device, (void)userdata1, (void)userdata2;
    fprintf(stderr, "[gpu] Error (type %d): %.*s\n", type, SV(message));

    // Reported on the next tick, not inside the faulty call: the message names the call and labels.
    if (IsDebuggerPresent()) {
        __debugbreak();
    }
}

bool Gpu_Init(Gpu* gpu, void* hinstance, void* hwnd, uint32_t width, uint32_t height, WGPUBackendType backend)
{
    // TODO(§1) Instance, with the TimedWaitAny instance feature.
    {
        WGPUInstanceDescriptor instance_desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
        WGPUInstanceFeatureName features[] = { WGPUInstanceFeatureName_TimedWaitAny };
        instance_desc.requiredFeatureCount = ARRAY_SIZE(features);
        instance_desc.requiredFeatures = features;
        gpu->instance = wgpuCreateInstance(&instance_desc);
        if (!gpu->instance) {
            return false;
        }
    }

    // TODO(§2) Surface from hinstance + hwnd.
    {
        WGPUSurfaceSourceWindowsHWND hwndDesc = {
            .chain = { .sType = WGPUSType_SurfaceSourceWindowsHWND, },
            .hinstance = hinstance,
            .hwnd = hwnd,
        };

        WGPUSurfaceDescriptor desc = { .nextInChain = &hwndDesc.chain , .label = Gpu_Str("Dooooooom") };
        gpu->surface = wgpuInstanceCreateSurface(gpu->instance, &desc);
        if (!gpu->surface) {
            return false;
        }
    }

    // TODO(§3) Adapter compatible with the surface, on `backend`.
    {
        WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
        options.featureLevel = WGPUFeatureLevel_Undefined;
        options.powerPreference = WGPUPowerPreference_HighPerformance;
        options.forceFallbackAdapter = false;
        options.backendType = backend;
        options.compatibleSurface = gpu->surface;


        WGPURequestAdapterCallbackInfo callback_info = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
        callback_info.mode = WGPUCallbackMode_WaitAnyOnly;
        callback_info.callback = on_adapter_get;
        callback_info.userdata1 = &gpu->adapter;
        WGPUFuture adapter_future = wgpuInstanceRequestAdapter(gpu->instance, &options, callback_info);

        {
            WGPUFutureWaitInfo wait_info = WGPU_FUTURE_WAIT_INFO_INIT;
            wait_info.future = adapter_future;
            WGPUWaitStatus status = wgpuInstanceWaitAny(gpu->instance, 1, &wait_info, UINT64_MAX);
            assert(status == WGPUWaitStatus_Success);
        }

        WGPUAdapterInfo adapter_info = WGPU_ADAPTER_INFO_INIT;
        wgpuAdapterGetInfo(gpu->adapter, &adapter_info);

        printf("adapter info:\n");
        printf("\tvendor: %.*s\n", SV(adapter_info.vendor));
        printf("\tarchitecture: %.*s\n", SV(adapter_info.architecture));
        printf("\tdevice: %.*s\n", SV(adapter_info.device));
        printf("\tdescription: %.*s\n", SV(adapter_info.description));

        wgpuAdapterInfoFreeMembers(adapter_info);
    }

    // TODO(§4) Device, queue, error callbacks, Dawn toggles.
    {
        WGPUDawnTogglesDescriptor toggles = WGPU_DAWN_TOGGLES_DESCRIPTOR_INIT;
        const char * enabled_toggles[] = { "use_user_defined_labels_in_backend", "disable_symbol_renaming" };
        toggles.enabledToggleCount = ARRAY_SIZE(enabled_toggles);
        toggles.enabledToggles = enabled_toggles;
        WGPUDeviceDescriptor device_desc = WGPU_DEVICE_DESCRIPTOR_INIT;
        device_desc.nextInChain = &toggles.chain;
        device_desc.label = Gpu_Str("MyGpu");
        device_desc.defaultQueue.label = Gpu_Str("MyQueue");
        device_desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
        device_desc.deviceLostCallbackInfo.callback = on_device_lost;
        device_desc.uncapturedErrorCallbackInfo.callback = on_uncaptured_error;
        
        WGPURequestDeviceCallbackInfo callback_info = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
        callback_info.mode = WGPUCallbackMode_WaitAnyOnly;
        callback_info.callback = on_device_create;
        callback_info.userdata1 = &gpu->device;
        WGPUFuture gpu_future = wgpuAdapterRequestDevice(gpu->adapter, &device_desc, callback_info);
        {
            WGPUFutureWaitInfo wait_info = WGPU_FUTURE_WAIT_INFO_INIT;
            wait_info.future = gpu_future;
            WGPUWaitStatus status = wgpuInstanceWaitAny(gpu->instance, 1, &wait_info, UINT64_MAX);
            assert(status == WGPUWaitStatus_Success);
        }
        
        gpu->queue = wgpuDeviceGetQueue(gpu->device);
    }

    // TODO(§5) Surface format, then Gpu_Resize(gpu, width, height).
    {
        WGPUSurfaceCapabilities caps = WGPU_SURFACE_CAPABILITIES_INIT;
        WGPUStatus status = wgpuSurfaceGetCapabilities(gpu->surface, gpu->adapter, &caps);
        ASSERT_STATUS_SUCCESS(status);
        gpu->surfaceFormat = caps.formats[0];
        
        Gpu_Resize(gpu, width, height);

        wgpuSurfaceCapabilitiesFreeMembers(caps);
    }


    return true;
}

void Gpu_Resize(Gpu* gpu, uint32_t width, uint32_t height)
{
    // TODO(§5) wgpuSurfaceConfigure. Skip 0x0 (minimized).

    if (width == 0 || height == 0) {
        return;
    }

    WGPUSurfaceConfiguration config = WGPU_SURFACE_CONFIGURATION_INIT;
    config.device = gpu->device;
    config.format = gpu->surfaceFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = width;
    config.height = height;
    config.presentMode = WGPUPresentMode_Immediate;

    gpu->width = width;
    gpu->height = height;

    wgpuSurfaceConfigure(gpu->surface, &config);
}

WGPUTextureView Gpu_BeginFrame(Gpu* gpu)
{
    // TODO(§5) wgpuSurfaceGetCurrentTexture, handle its status, return a view.
    WGPUSurfaceTexture surface_texture = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(gpu->surface, &surface_texture);
    switch (surface_texture.status)
    {
        case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
        case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
            break;

        case WGPUSurfaceGetCurrentTextureStatus_Outdated:
        case WGPUSurfaceGetCurrentTextureStatus_Lost:
            if (surface_texture.texture) {
                wgpuTextureRelease(surface_texture.texture);
            }
            Gpu_Resize(gpu, gpu->width, gpu->height);
            return NULL;

        default:
            if (surface_texture.texture) {
                wgpuTextureRelease(surface_texture.texture);
            }
            return NULL;
    }

    gpu->frameTexture = surface_texture.texture;
    
    WGPUTextureViewDescriptor viewDesc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    viewDesc.label = Gpu_Str("Swapchain view");
    return wgpuTextureCreateView(surface_texture.texture, &viewDesc);
}

void Gpu_EndFrame(Gpu* gpu, WGPUTextureView view)
{
    // TODO(§5) Present, release the view and the surface texture.
    wgpuSurfacePresent(gpu->surface);

    wgpuTextureViewRelease(view);
    wgpuTextureRelease(gpu->frameTexture);
    gpu->frameTexture = NULL;

    wgpuInstanceProcessEvents(gpu->instance);
}

void Gpu_Shutdown(Gpu* gpu)
{
    // TODO(§6) Release everything Gpu_Init created.
    wgpuSurfaceUnconfigure(gpu->surface);
    wgpuDeviceRelease(gpu->device);
    wgpuAdapterRelease(gpu->adapter);
    wgpuInstanceRelease(gpu->instance);
}
