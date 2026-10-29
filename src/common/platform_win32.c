#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "platform_win32.h"

Platform g_platform;

static bool s_quit;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        g_platform.width   = LOWORD(lParam);
        g_platform.height  = HIWORD(lParam);
        g_platform.resized = true;
        return 0;

    // Sys keys are Alt combos. Swallow them (Alt alone would open the window menu and
    // freeze input), except Alt+F4.
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        if (g_platform.onKey)
            g_platform.onKey(msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN, (unsigned int)wParam);
        if (msg == WM_SYSKEYDOWN && wParam == VK_F4)
            break;
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        s_quit = true;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

bool Platform_CreateWindow(const char* title, uint32_t width, uint32_t height)
{
    // Physical pixels: no blurry DWM upscaling on high-DPI monitors.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HINSTANCE hinstance = GetModuleHandleA(NULL);

    WNDCLASSEXA wc   = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = "WebGpuWindow";
    if (!RegisterClassExA(&wc))
        return false;

    // Grow the window rect so the client area, not the frame, is width x height.
    RECT rect = { 0, 0, (LONG)width, (LONG)height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, hinstance, NULL);
    if (!hwnd)
        return false;

    g_platform.hinstance = hinstance;
    g_platform.hwnd      = hwnd;
    g_platform.width     = width;
    g_platform.height    = height;

    // ShowWindow sends WM_SIZE. The initial size is known, nothing to reconfigure yet.
    ShowWindow(hwnd, SW_SHOW);
    g_platform.resized = false;

    return true;
}

bool Platform_PumpMessages(void)
{
    MSG msg;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return !s_quit;
}

void Platform_SetTitle(const char* title)
{
    if (g_platform.hwnd)
        SetWindowTextA(g_platform.hwnd, title);
}
