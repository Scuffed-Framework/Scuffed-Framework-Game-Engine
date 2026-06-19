#pragma once
#include <Graphics/Pipelines/Pipeline.hpp>
#include <Graphics/PipelinePassManager.hpp> // fully defined, no cycle
#include <functional>
#include <vector>

namespace SF::Engine
{
    class SceneRenderer;

    class PipelinePassRegistry
    {
    public:
        using Factory = std::function<void(PipelinePassManager &)>; // ← manager, not renderer

        static PipelinePassRegistry &Get()
        {
            static PipelinePassRegistry instance;
            return instance;
        }

        void Register(Factory factory)
        {
            factories_.push_back(std::move(factory));
        }

        template <typename TPass, typename... Args>
        void Register(Pipeline::Stage stage, Args &&...args)
        {
            Register([stage, ... args = std::forward<Args>(args)](PipelinePassManager &mgr) mutable
                     { mgr.Add<TPass>(stage, std::make_unique<TPass>(stage, std::forward<Args>(args)...)); });
        }

        // Called by SceneRenderer::Start() — defined in .cpp to avoid needing
        // SceneRenderer's full definition here.
        void Apply(SceneRenderer &renderer);

    private:
        std::vector<Factory> factories_;
    };
}