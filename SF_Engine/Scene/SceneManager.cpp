#include "SceneManager.hpp"
#include <Default/DefaultScene.hpp>

namespace SF::Engine
{
    SceneManager::SceneManager()
    {
        // Intentionally empty.
        // Engine::Engine() is responsible for constructing the startup scene
        // (DefaultScene or a loaded one) and handing it to us via SetScene().
        // Creating a DefaultScene here causes a double-construction:
        //   1. SceneManager() makes one scene, stores it in `scene`
        //   2. Engine::Engine() makes another and calls SetScene(), which
        //      move-assigns `scene`, destroying the first one mid-construction.
        // That destruction fires ~Scene() -> ~vector<SceneLight> -> ~SceneLight
        // -> vtable dispatch into garbage -> 0xC0000005.
    }

    void SceneManager::Update()
    {
        if (pendingScene)
        {
            if (scene)
                scene->Stop();

            scene = std::move(pendingScene);

            if (auto *gi = Engine::Get()->GetGameInstance())
                gi->OnSceneLoad(scene.get());
        }

        if (!scene)
            return;

        // Initialize deferred GPU resources (renderer, meshes, LightManager)
        // on the first Update() after all modules including RenderSystem are ready.
        if (scene)
        {
            if (!scene->initialized_)
                scene->Initialize();

            if (!scene->started_)
            {
                scene->Start();
                scene->started_ = true;
            }

            scene->Update();
            scene->Render();
        }
    }
}
