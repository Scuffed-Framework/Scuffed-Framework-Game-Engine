#pragma once
#include "Scene.hpp"
#include <UtilityClasses/UUID.hpp>
#include "SceneRenderer.hpp"

namespace SF::Engine
{
    // wire this in eventually
    struct SceneContext
    {
        enum class Type
        {
            Editor,
            PIE, // Play-In-Editor instance
            Game,
            Preview,
            Server,
            SubWorld
        };

        Type type;

        // Owning world/scene
        Scene *scene = nullptr;

        // Unique ID (PIE worlds need multiple copies)
        UUID id = UUID::Generate();

        // Runtime state
        bool isPaused = false;
        bool isInitialized = false;

        // For PIE/debugging
        bool simulateInEditor = false;

        // Renderer for this world
        SceneRenderer *renderer = nullptr;

        // Optional camera override (e.g. editor camera)
        Camera *overrideCamera = nullptr;

        // Time values (per world!)
        float deltaTime = 0.f;
        float elapsed = 0.f;
    };
}