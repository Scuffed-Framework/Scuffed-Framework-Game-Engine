#pragma once
#include <Rendering/PipelinePassManager.hpp>
#include <Rendering/RHI/Buffers/UniformBuffer.hpp>
#include <Rendering/RHI/Descriptors/DescriptorSet.hpp>
#include <Rendering/RHI/Images/Cubemap.hpp>
#include <Rendering/RHI/Pipelines/RenderPipeline.hpp>
#include <memory>

#include <Rendering/PipelinePassInit.hpp>
#include <Rendering/Visuals/sfSkies/TimeManager.hpp>

namespace SF::Engine
{
    class SkyboxPipelinePass : public PipelinePass
    {
        inline static bool s_registered = []()
        {
            PipelinePassInitRegistry::Get().Register(
                    [](PipelinePassManager &mgr)
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

        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;
    };
}