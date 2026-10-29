#pragma once

#include <webgpu/webgpu.h>

// Loads WGSL source from shaders/. Not WebGPU: plain file I/O.

// name: path relative to shaders/, e.g. "triangle.wgsl".
// Returns the null-terminated file text, or NULL on error (printed to stderr).
// Read at run time from the repository, so editing a shader only needs a restart.
WGPUShaderModule Shader_Load(WGPUDevice device, const char* name);

// Frees text returned by Shader_Load. Accepts NULL.
void Shader_Free(WGPUShaderModule module);
