#include <Engine/Engine.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Rendering/Windows/WindowManager.hpp>
#include <Gui/ImGuiPipelinePass.hpp>
#include <Scene/SceneManager.hpp>

#include <filesystem>
#include <iostream>

#include "LevelEditor.hpp"

int main()
{
    SF::Engine::EditorApplication app;

    auto result = app.Init();

    std::cout << "Init result: " << result << '\n';
    std::cout << "Window: " << app.window << '\n';

    if (result != SF::Engine::EditorApplication::Success)
        return -1;

    std::cout << "IsClosed: " << app.window->IsClosed() << '\n';

    app.AppLoop();

    std::cout << "AppLoop returned\n";

    return 0;
}