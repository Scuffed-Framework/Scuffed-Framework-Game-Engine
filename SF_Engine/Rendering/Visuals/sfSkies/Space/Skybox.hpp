#pragma once
#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>
#include <Rendering/RHI/Buffers/UniformBuffer.hpp>
#include <Rendering/RHI/Descriptors/DescriptorSet.hpp>
#include <Rendering/RHI/Images/Cubemap.hpp>
#include <Rendering/RHI/Pipelines/RhiRenderPipeline.hpp>
#include <memory>

#include <Rendering/FrameGraph/EngineRenderpassInitRegistry.hpp>
#include <Rendering/Visuals/sfSkies/TimeManager.hpp>

namespace SF::Engine
{
    class SkyboxPipelinePass : public EngineRenderpass
    {
        inline static bool s_registered = []()
        {
            EngineRenderpassInitRegistry::Get().Register(
                    [](EngineRenderpassManager &mgr)
                    {
                        mgr.Add<SkyboxPipelinePass>(Pipeline::Stage{0, 0},
                                                    std::make_unique<SkyboxPipelinePass>(Pipeline::Stage{0, 0}));
                    });
            return true;
        }();

    public:
        explicit SkyboxPipelinePass(Pipeline::Stage stage);

        ~SkyboxPipelinePass() override = default;

        void Render(const CommandBuffer &commandBuffer) override;

    private:
        std::unique_ptr<Cubemap> cubemap_;

        std::unique_ptr<RhiRenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;
    };
}