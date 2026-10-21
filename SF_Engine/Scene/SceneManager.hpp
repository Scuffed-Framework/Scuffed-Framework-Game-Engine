#pragma once

#include <Engine/Engine.hpp>
#include <Rendering/RenderSystem.hpp>
#include "Scene.hpp"

#include <filesystem>
#include <functional>
#include <optional>

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
        [[nodiscard]] Scene *GetScene() const { return scene.get(); }

        /**
         * Sets the current scene to a new scene. Takes effect at the start of the next Update().
         * @param newScene The new scene.
         */
        void SetScene(std::unique_ptr<Scene> &&newScene)
        {
            pendingScene = std::move(newScene);
            pendingOpen.reset(); // a queued open targeted the scene we're about to replace
        }

        /**
         * Queues loading `file` into the active scene. Executed at the top of the next Update(),
         * before systems update or the scene renders, so it never mutates entities mid-frame.
         */
        void RequestOpen(std::filesystem::path file) { pendingOpen = std::move(file); }

        [[nodiscard]] bool IsSceneStarted() const { return scene && scene->started_; }

    private:
        void ApplyPendingOpen();

        std::unique_ptr<Scene> scene;
        std::unique_ptr<Scene> pendingScene;
        std::optional<std::filesystem::path> pendingOpen;
    };

    inline Scene *GetCurrentScene() { return SceneManager::Get()->GetScene(); }

    const std::filesystem::path &CurrentScenePath(); // file the active scene was loaded from / saved to
    bool CurrentScenePathNotEmpty();
    bool IsFileCurrentCurrentScene(const std::filesystem::path &file);

    // Validates the file and queues the load; returns true if queued. The actual load (and the
    // OnSceneReplaced callback) happens at the start of the next SceneManager::Update().
    bool OpenScene(const std::filesystem::path &file);
    bool SaveScene(); // save to CurrentScenePath(); false if none or failed
    // Appends ".scene" if `file` doesn't already have that extension.
    bool SaveSceneAs(const std::filesystem::path &file);

    // Writes a new scene file containing the default scene (sun, point light, cubes).
    // Returns the path used (made unique if needed), or empty on failure.
    std::filesystem::path CreateDefaultScene(const std::filesystem::path &dir, const std::string &name);

    void NotifyMovedScene(const std::filesystem::path &from, const std::filesystem::path &to);
    void NotifyDeletedScene(const std::filesystem::path &file);

    // Called after a scene's entities are replaced, either by an OpenScene() load or a SetScene() swap
    // (clear selections that hold Entity*).
    void SetOnSceneReplaced(std::function<void()> cb);
} // namespace SF::Engine
