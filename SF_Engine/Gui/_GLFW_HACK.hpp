// GLFW_HACK.hpp - Just include this ONCE in your project, that's it.
// Put this BEFORE any GLFW includes in ONE .cpp file.
//
// Usage:
//   #include "GLFW_HACK.hpp"
//   // That's it. No other includes needed.

#pragma once

#ifdef _WIN32

// Define this before GLFW sees it
#define GLFW_EXPOSE_NATIVE_WIN32

// Include GLFW
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <malloc.h>
#include <string.h> // for memset
#include <windows.h>

#pragma comment(lib, "dwmapi.lib")

// Your custom WndProc callback
// Return the result to send back, or call the originalProc to chain
typedef LRESULT(CALLBACK *GLFW_WndProc_Callback)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                 GLFWwindow *glfwWindow, WNDPROC originalGLFWProc);

// Install a WndProc hook on a GLFW window
// Returns true on success
inline bool GLFW_HookWndProc(GLFWwindow *window, GLFW_WndProc_Callback callback);

// Remove WndProc hook
inline void GLFW_UnhookWndProc(GLFWwindow *window);

// Get the original GLFW WndProc
inline WNDPROC GLFW_GetOriginalWndProc(GLFWwindow *window);

namespace GLFW_Hack_Internal
{

    struct HookData
    {
        GLFWwindow *glfwWindow;
        WNDPROC originalProc;
        GLFW_WndProc_Callback userCallback;
    };

    static const wchar_t *HOOK_PROP_NAME = L"GLFW_WndProc_Hook_Data_Ptr";

    static LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        HookData *hook = (HookData *)GetPropW(hwnd, HOOK_PROP_NAME);

        if (hook && hook->userCallback)
        {
            return hook->userCallback(hwnd, msg, wParam, lParam, hook->glfwWindow,
                                      hook->originalProc);
        }

        if (hook && hook->originalProc)
        {
            return CallWindowProcW(hook->originalProc, hwnd, msg, wParam, lParam);
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

inline bool GLFW_HookWndProc(GLFWwindow *window, GLFW_WndProc_Callback callback)
{
    if (!window || !callback)
        return false;

    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd)
        return false;

    using namespace GLFW_Hack_Internal;

    HookData *hook = (HookData *)GetPropW(hwnd, HOOK_PROP_NAME);

    if (!hook)
    {
        // Use malloc/free to avoid DLL boundary issues with new/delete
        hook = (HookData *)malloc(sizeof(HookData));
        if (!hook)
            return false;
        memset(hook, 0, sizeof(HookData));

        hook->glfwWindow = window;
        hook->originalProc =
            (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)HookedWndProc);
        SetPropW(hwnd, HOOK_PROP_NAME, (HANDLE)hook);
    }

    hook->userCallback = callback;
    return true;
}

inline void GLFW_UnhookWndProc(GLFWwindow *window)
{
    if (!window)
        return;

    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd)
        return;

    using namespace GLFW_Hack_Internal;

    HookData *hook = (HookData *)GetPropW(hwnd, HOOK_PROP_NAME);
    if (hook)
    {
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)hook->originalProc);
        RemovePropW(hwnd, HOOK_PROP_NAME);
        free(hook); // Use free() to match malloc()
    }
}

inline WNDPROC GLFW_GetOriginalWndProc(GLFWwindow *window)
{
    if (!window)
        return nullptr;

    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd)
        return nullptr;

    using namespace GLFW_Hack_Internal;

    HookData *hook = (HookData *)GetPropW(hwnd, HOOK_PROP_NAME);
    return hook ? hook->originalProc : nullptr;
}

// Makes a borderless window with custom titlebar
#define GLFW_HACK_ENABLE_BORDERLESS(hwnd)                                        \
    do                                                                           \
    {                                                                            \
        LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);                     \
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;                                 \
        SetWindowLongPtrW(hwnd, GWL_STYLE, style);                               \
        MARGINS m = {0, 0, 0, 1};                                                \
        DwmExtendFrameIntoClientArea(hwnd, &m);                                  \
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,                                     \
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER); \
    } while (0)

// Quick hit test for titlebar dragging
#define GLFW_HACK_HITTEST_TITLEBAR(cursorY, titleBarHeight) \
    ((cursorY) >= 0 && (cursorY) < (titleBarHeight) ? HTCAPTION : HTCLIENT)

// Quick hit test for resize borders
#define GLFW_HACK_HITTEST_BORDER(cursorX, cursorY, width, height, borderWidth) \
    ((cursorX) < (borderWidth)              ? HTLEFT                           \
     : (cursorX) > (width) - (borderWidth)  ? HTRIGHT                          \
     : (cursorY) < (borderWidth)            ? HTTOP                            \
     : (cursorY) > (height) - (borderWidth) ? HTBOTTOM                         \
                                            : HTCLIENT)

#endif // _WIN32