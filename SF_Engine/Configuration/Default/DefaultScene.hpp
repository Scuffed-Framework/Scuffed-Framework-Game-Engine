#pragma once
#include <Controllers/CameraController.hpp>
#include <Scene/Scene.hpp>

namespace SF::Engine
{
    struct TemplateScene : Scene
    {
        explicit TemplateScene(const std::string &name) :
            Scene(std::unique_ptr<CameraController>{}, name, SceneRendererConfig{}, /*populateDefaults=*/true)
        {
        }
        void Start() override {}
        bool IsPaused() const override { return false; }
    };

    /**
     * @brief Ready-to-use scene shown when no startup scene is specified.
     *
     * Mirrors the Unity default scene:
     *   - EditorCamera at (0, 1.5, 6) looking toward origin
     *   - Grey metallic cube at the origin
     *   - Directional sun light
     *   - Atmosphere pass
     *   - Sun disc pass
     *
     * Replace it at any time with SceneManager::SetScene().
     */
    class DefaultScene : public Scene
    {
    public:
        DefaultScene() :
            Scene(std::make_unique<CameraController>(), "Default Scene", SceneRendererConfig{.enableAtmosphere = true})
        {
        }

        explicit DefaultScene(const string &name) :
            Scene(std::make_unique<CameraController>(), name, SceneRendererConfig{.enableAtmosphere = true})
        {
        }

        void Start() override {}

        bool IsPaused() const override { return false; }
    };
} // namespace SF::Engine
