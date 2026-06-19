#pragma once
#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Buffers/UniformBuffer.hpp>
#include <Graphics/Images/Cubemap.hpp>
#include <memory>

#include <Graphics/Visuals/sfSkies/TimeManager.hpp>
#include <Graphics/PipelinePassInit.hpp>

namespace SF::Engine
{
    class SkyboxPipelinePass : public PipelinePass
    {
        inline static bool s_registered = []()
        {
            PipelinePassInitRegistry::Get().Register(
                [](PipelinePassManager &mgr)
                {
                    mgr.Add<SkyboxPipelinePass>(
                        Pipeline::Stage{0, 0},
                        std::make_unique<SkyboxPipelinePass>(
                            Pipeline::Stage{0, 0}));
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