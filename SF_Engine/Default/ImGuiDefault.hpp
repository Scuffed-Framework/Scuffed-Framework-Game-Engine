#pragma once

// Always use the engine-local ImGui copy, never the Conan-installed one.
// The engine context is created with ocornut/imgui so all ImGuiStyle access
// must go through the same header to guarantee matching struct layout.
#include <ImGui/ocornut/imgui.h>

namespace SF::Engine
{
    class ImGuiDefaultStyle
    {
    public:
        static void SetStyle();
        static void SetStyle2();
    };
}
