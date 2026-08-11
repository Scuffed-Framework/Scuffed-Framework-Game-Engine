#include <Engine/Engine.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Rendering/Windows/WindowManager.hpp>
#include <Gui/ImGuiPipelinePass.hpp>
#include <Scene/SceneManager.hpp>

#include <filesystem>
#include <iostream>

#include "LevelEditor.hpp"

int main(int argc, char *argv[])
{
    std::cout << "hi\n";
    // Change working directory to wherever the exe lives
    // so relative paths like "Shaders/Cube.shader" resolve correctly
    if (argc > 0)
    {
        auto exeDir = std::filesystem::weakly_canonical(
                          std::filesystem::path(argv[0]))
                          .parent_path();
        std::filesystem::current_path(exeDir);
    }

    std::cout << "Starting SF Engine\n";
    try
    {
        // Engine creates all registered modules (WindowManager, RenderSystem,
        // SceneManager, etc.) and sets up the DefaultScene.
        SF::Engine::Engine engine(argv[0]);
        std::cout << "Engine initialized\n";

        auto *wndMgr = SF::Engine::WindowManager::Get();
        auto *renderer = SF::Engine::RenderSystem::Get();
        auto *sceneMgr = SF::Engine::SceneManager::Get();

        if (!wndMgr || !renderer || !sceneMgr)
        {
            std::cerr << "Failed to get engine modules\n";
            return 1;
        }

        SF::Engine::Window *window = wndMgr->AddWindow();
        if (!window)
        {
            std::cerr << "Failed to create window\n";
            return 1;
        }

        auto version = engine.GetVersion();

        window->SetTitle(
            std::string("SF Engine Version: ") + std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch));
        window->SetResizable(true);
        window->SetBorderColor(SF::Engine::Color(1.0f, 0.48f, 0.0f, 1.0f));
        window->SetTitleColor(SF::Engine::Color());

        std::cout << "Window ready\n";

        // Main loop  drive all module stages in the correct order:
        //   Normal  : SceneManager (Initialize / Start / Update+Render)
        //   Render  : RenderSystem (pipeline pass execution + present)
        while (!window->IsClosed())
        {
            wndMgr->Update();   // polls GLFW events
            sceneMgr->Update(); // Stage::Normal  scene logic + mesh submission
            renderer->Update(); // Stage::Render  Vulkan draw + present
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Unhandled exception: " << e.what() << '\n';
        return 1;
    }

    std::cout << "Shutting down\n";
    return 0;
}
