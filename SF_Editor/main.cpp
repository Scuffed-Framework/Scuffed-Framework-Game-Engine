#include <Engine/Engine.hpp>
#include <Gui/ImGui/ImGuiPipelinePass.hpp>
#include <Platform/Windowing/WindowManager.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Scene/SceneManager.hpp>

#include <filesystem>
#include <iostream>

#include "Application/LevelEditor.hpp"

int main()
{
    SF::Engine::EditorApplication app;

    if (auto result = app.Init(); result != SF::Engine::EditorApplication::Success)
        return -1;

    app.AppLoop();

    return 0;
}
