#include "Engine.hpp"
#include <Configuration/Default/DefaultScene.hpp>
#include <Scene/SceneManager.hpp>
#include <Engine/Project/Project.hpp>
#include <GameScript/LuaEngine.hpp>

#ifdef Always
#undef Always
#endif

namespace SF::Engine
{
    Engine *Engine::Instance = nullptr;

    Engine::Engine(std::string argv0, ModuleFilter &&moduleFilter, URL startUpURL)
        : argv0(std::move(argv0)),
          version{Engine_VERSION_MAJOR, Engine_VERSION_MINOR, Engine_VERSION_PATCH},
          fpsLimit(-1.0f),
          running(true),
          elapsedUpdate(15.77ms),
          elapsedRender(-1s)
    {
        Instance = this;
        Log::Init(ApplicationTime::GetDateTime("Logs/%Y%m%d%H%M%S.txt"));

        // Create modules from registry
        for (auto it = Module::Registry().begin(); it != Module::Registry().end(); ++it)
            CreateModule(it, moduleFilter);

        // Initialize all modules
        for (auto &[id, module] : modules)
        {
            if (module && !module->Initialize())
            {
                Log::Error("Failed to initialize module: {}", module->GetName());
            }
        }

        // annoying, getting the logical device inside the RenderSystem constructor is too early,
        // so we have to call this after the RenderSystem is fully initialized.
        RenderSystem::Get()->PostInit();

        gameInstance = std::make_unique<GameInstance>();
        // Load the startup scene, falling back to DefaultScene on any failure.
        // SceneManager::Update() will call Start() on the first frame  don't call it here.
        std::unique_ptr<Scene> startupScene;

        // Only attempt a network/file load if the URL isn't the placeholder default
        const bool isDefaultURL = (startUpURL.scheme == "map" &&
                                   startUpURL.authority == "startup");
        if (!isDefaultURL)
        {
            try
            {
                SceneLoadResult result = resolver.Load(startUpURL);
                if (result.success)
                {
                    startupScene = std::move(result.scene);
                    Log::Info("Engine: startup scene loaded from '{}'",
                              startUpURL.ToString());
                    if (gameInstance)
                        gameInstance->OnCreate();
                }
                else
                {
                    Log::Warning("Engine: startup scene load failed ({}), "
                                 "using DefaultScene",
                                 result.error);
                }
            }
            catch (const std::exception &e)
            {
                Log::Warning("Engine: startup scene exception ({}), "
                             "using DefaultScene",
                             e.what());
            }
        }

        if (!startupScene)
            startupScene = std::make_unique<DefaultScene>();

        // Hand the scene to SceneManager  it owns and updates it from here
        if (auto *sm = SceneManager::Get())
        {
            sm->SetScene(std::move(startupScene));
        }
    }

    Engine::~Engine()
    {
        // Clear layer stack before destroying app
        layerStack.Clear();

        // Shutdown modules in reverse order
        for (auto it = modules.rbegin(); it != modules.rend(); ++it)
        {
            if (it->second)
                it->second->Shutdown();
        }

        // Destroy modules - collect keys first, since DestroyModule erases from the map
        // and iterating + erasing simultaneously invalidates the reverse iterator
        while (!modules.empty())
        {
            auto id = modules.rbegin()->first;
            DestroyModule(id);
        }

        Log::Shutdown();
        Instance = nullptr;
    }

    int32_t Engine::Run()
    {
        while (running)
        {
            elapsedRender.SetInterval(ApplicationTime::Seconds(1.0f / fpsLimit));

            // Always-Update.
            UpdateStage(Module::Stage::Always);

            if (elapsedUpdate.GetElapsed() != 0)
            {
                // Resets the timer.
                ups.Update(ApplicationTime::Now());

                UpdateStage(Module::Stage::Pre);

                layerStack.UpdateLayers();

                UpdateStage(Module::Stage::Normal);
                UpdateStage(Module::Stage::Post);

                deltaUpdate.Update();
            }

            // Renders when needed.
            if (elapsedRender.GetElapsed() != 0)
            {
                fps.Update(ApplicationTime::Now());

                UpdateStage(Module::Stage::Render);

                deltaRender.Update();
            }
        }
        vkDeviceWaitIdle(*RenderSystem::Get()->GetLogicalDevice());

        return EXIT_SUCCESS;
    }

    void Engine::CreateModule(RegMap::const_iterator it, const ModuleFilter &filter)
    {
        // Check if module already exists
        if (modules.find(it->first) != modules.end())
            return;

        // Check if module is filtered out
        if (!filter.Check(it->first))
            return;

        // Recursively create dependencies first
        for (auto requireId : it->second.dependencies)
        {
            auto depIt = Module::Registry().find(requireId);
            if (depIt != Module::Registry().end())
                CreateModule(depIt, filter);
            else
                Log::Warning("Module dependency not found: TypeId {}", requireId);
        }

        // Create the module instance using the factory function
        auto module = it->second.createFunc();

        if (module)
        {
            Log::Info("Creating module: {}", it->second.name);
            modules[it->first] = std::move(module);
            moduleStages[it->second.stage].emplace_back(it->first);
        }
        else
        {
            Log::Error("Failed to create module: {}", it->second.name);
        }
    }

    void Engine::DestroyModule(TypeId id)
    {
        auto it = modules.find(id);
        if (it == modules.end() || !it->second)
            return;

        // Capture stage BEFORE any recursion invalidates the iterator
        auto stage = it->second->GetStage();

        for (const auto &[registrarId, registrar] : Module::Registry())
        {
            auto depIt = std::find(registrar.dependencies.begin(), registrar.dependencies.end(), id);
            if (depIt != registrar.dependencies.end())
                DestroyModule(registrarId);
        }

        // Re-find after recursion since the map may have changed
        it = modules.find(id);
        if (it == modules.end())
            return; // already destroyed by recursive call

        auto &stageVec = moduleStages[stage]; // use captured stage, not it->second
        stageVec.erase(std::remove(stageVec.begin(), stageVec.end(), id), stageVec.end());

        modules.erase(it);
    }

    void Engine::UpdateStage(ModuleStage stage)
    {
        auto stageIt = moduleStages.find(stage);
        if (stageIt == moduleStages.end())
            return;

        for (auto &moduleId : stageIt->second)
        {
            auto modIt = modules.find(moduleId);
            if (modIt != modules.end() && modIt->second)
                modIt->second->Update();
        }
    }
}