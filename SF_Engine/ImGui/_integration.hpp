#pragma once

// SF Engine always uses GLFW as its windowing layer, so we use the GLFW
// ImGui backend on all platforms. The Win32 backend is NOT included here :
// it requires manual WndProc hooking and conflicts with GLFW's own hook.

#include <GLFW/glfw3.h>
#include "ocornut/imgui.h"
#include "ocornut/imgui_impl_glfw.h"
#include "ocornut/imgui_impl_vulkan.h"
