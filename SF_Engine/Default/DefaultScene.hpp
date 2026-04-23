#pragma once
#include <Scene/Scene.hpp>
#include <Camera/EditorCamera.hpp>

namespace SF::Engine
{
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
        DefaultScene()
            : Scene(std::make_unique<EditorCamera>(),
                    SceneRendererConfig{
                        .enableAtmosphere = true,
                        .enableSun = true,
                        .enableClouds = true,
                    })
        {
        }

        void Start() override
        {
        }

        bool IsPaused() const override { return false; }
    };
}