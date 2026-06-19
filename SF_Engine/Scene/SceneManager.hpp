#pragma once

#include <Engine/Engine.hpp>
#include <Graphics/RenderSystem.hpp>
#include <TemplateLibrary/Dynamic.hpp>
#include "Scene.hpp"

namespace SF::Engine
{
    /**
     * @brief Module used for managing game scenes.
     */
    class SceneManager : public ModuleRegistrar<SceneManager>
    {
        REGISTER_MODULE(SceneManager, ModuleStage::Normal, Requires<RenderSystem>{});

    public:
        SceneManager();

        void Update() override;
        /**
         * Gets the current scene.
         * @return The current scene.
         */
        Scene *GetScene() const
        {
            return scene.get();
        }

        /**
         * Sets the current scene to a new scene.
         * @param scene The new scene.
         */
        void SetScene(std::unique_ptr<Scene> &&newScene)
        {
            pendingScene = std::move(newScene);
            sceneStarted = false;
        }

        bool IsSceneStarted() { return sceneStarted; }

    private:
        std::unique_ptr<Scene> scene;
        std::unique_ptr<Scene> pendingScene;
        bool sceneStarted = false;
    };
}
