#pragma once

// Minimal Win32 window + keyboard input.
// Not WebGPU: provided so all your time goes to the GPU side.

#include <stdbool.h>
#include <stdint.h>

typedef void (*Platform_KeyFn)(bool pressed, unsigned int virtualKey);

typedef struct Platform
{
    void*          hinstance;  // HINSTANCE, for WGPUSurfaceSourceWindowsHWND
    void*          hwnd;       // HWND
    uint32_t       width;      // client area in pixels, 0 when minimized
    uint32_t       height;
    bool           resized;    // set on WM_SIZE, cleared by the app once the surface is reconfigured
    Platform_KeyFn onKey;      // optional
} Platform;

extern Platform g_platform;

// Client area is width x height physical pixels (process is per-monitor DPI aware).
bool Platform_CreateWindow(const char* title, uint32_t width, uint32_t height);

// Drains pending window messages. Returns false once the window is closed.
bool Platform_PumpMessages(void);

void Platform_SetTitle(const char* title);
