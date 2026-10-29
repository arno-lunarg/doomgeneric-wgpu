// doomgeneric platform backend: Win32 window, keyboard, timer.
// Frames are blitted with GDI for now. Tutorial step 2 replaces DG_DrawFrame's blit with WebGPU.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "doomgeneric.h"
#include "doomkeys.h"

#include "platform_win32.h"

// Doom ran 320x200 on 4:3 CRTs: pixels are 20% taller than wide.
#define DISPLAY_ASPECT (4.0f / 3.0f)

#define KEYQUEUE_SIZE 16

static unsigned short s_keyQueue[KEYQUEUE_SIZE];
static unsigned int   s_keyQueueWrite;
static unsigned int   s_keyQueueRead;

static LARGE_INTEGER s_timerFrequency;
static LARGE_INTEGER s_timerStart;

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
    s_keyQueueWrite             = (s_keyQueueWrite + 1) % KEYQUEUE_SIZE;
}

// Largest DISPLAY_ASPECT rect centered in the client area, black bars around it.
static void BlitGdi(void)
{
    HWND hwnd = g_platform.hwnd;
    HDC  hdc  = GetDC(hwnd);

    int clientW = (int)g_platform.width;
    int clientH = (int)g_platform.height;
    int w       = clientW;
    int h       = (int)(clientW / DISPLAY_ASPECT);
    if (h > clientH)
    {
        h = clientH;
        w = (int)(clientH * DISPLAY_ASPECT);
    }
    int x = (clientW - w) / 2;
    int y = (clientH - h) / 2;

    PatBlt(hdc, 0, 0, x, clientH, BLACKNESS);
    PatBlt(hdc, x + w, 0, clientW - x - w, clientH, BLACKNESS);
    PatBlt(hdc, x, 0, w, y, BLACKNESS);
    PatBlt(hdc, x, y + h, w, clientH - y - h, BLACKNESS);

    // DG_ScreenBuffer is 0x00RRGGBB: BGRA bytes in memory, which is what a 32-bit DIB expects.
    BITMAPINFO bmi              = { 0 };
    bmi.bmiHeader.biSize        = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth       = DOOMGENERIC_RESX;
    bmi.bmiHeader.biHeight      = -DOOMGENERIC_RESY;  // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(hdc, x, y, w, h, 0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY, DG_ScreenBuffer, &bmi,
                  DIB_RGB_COLORS, SRCCOPY);

    ReleaseDC(hwnd, hdc);
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
}

void DG_DrawFrame(void)
{
    if (!Platform_PumpMessages())
        exit(0);

    if (g_platform.width == 0 || g_platform.height == 0)
        return;

    // Step 2: replace with a WebGPU texture upload + fullscreen draw.
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
    s_keyQueueRead         = (s_keyQueueRead + 1) % KEYQUEUE_SIZE;

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
