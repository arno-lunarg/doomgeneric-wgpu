#include <stdio.h>
#include <stdlib.h>

#include "shader.h"

#include "gpu.h"

// SHADER_DIR is the absolute shaders/ path with a trailing '/', defined in props/Common.props.
#ifndef SHADER_DIR
#error SHADER_DIR not defined
#endif

WGPUShaderModule Shader_Load(WGPUDevice device, const char* name)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s%s", SHADER_DIR, name);

    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Shader_Load: cannot open %s\n", path);
        return NULL;
    }

    // Size from the end offset. Binary mode: no CRLF translation, ftell is the byte count.
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // +1 for the terminator: WGPU_STRLEN expects a null-terminated string.
    char* text = malloc((size_t)size + 1);
    size_t read = text ? fread(text, 1, (size_t)size, file) : 0;
    fclose(file);

    if (!text || read != (size_t)size)
    {
        fprintf(stderr, "Shader_Load: cannot read %s\n", path);
        free(text);
        return NULL;
    }

    text[size] = '\0';
    
    WGPUShaderSourceWGSL wgsl = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl.code = Gpu_Str(text);

    WGPUShaderModuleDescriptor shader_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    shader_desc.nextInChain = &wgsl.chain;
    return wgpuDeviceCreateShaderModule(device, &shader_desc);
}

void Shader_Free(WGPUShaderModule module)
{
    wgpuShaderModuleRelease(module);
}
