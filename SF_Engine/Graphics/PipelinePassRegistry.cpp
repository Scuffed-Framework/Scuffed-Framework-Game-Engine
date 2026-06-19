#include <Graphics/PipelinePassRegistry.hpp>
#include <Scene/SceneRenderer.hpp> // full definition lives here, not in the header

namespace SF::Engine
{
    void PipelinePassRegistry::Apply(SceneRenderer &renderer)
    {
        // SceneRenderer exposes its PipelinePassManager
        for (auto &f : factories_)
            f(*renderer.GetPipelinePassManager());
    }
}