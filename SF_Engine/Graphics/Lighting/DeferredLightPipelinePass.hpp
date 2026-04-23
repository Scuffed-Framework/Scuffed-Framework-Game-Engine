#pragma once

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Images/ImageDepth.hpp>
#include "LightManager.hpp"
#include <memory>

namespace SF::Engine
{
    /**
     * @brief Fullscreen deferred lighting pass.
     *
     * Single descriptor set layout (all bindings flat, as SPIR-V reflection sees them):
     *   bind 0  UBO   GpuFrameData
     *   bind 1  SSBO  GpuLight[]
     *   bind 2  SSBO  GpuClusterLightList[]
     *   bind 3  SSBO  uint lightIndices[]
     *   bind 4  sampler2D gbufAlbedo
     *   bind 5  sampler2D gbufNormal
     *   bind 6  sampler2D gbufPBR
     *   bind 7  sampler2D gbufDepth
     *
     * GBuffer image descriptors are refreshed lazily when attachment pointers change
     * (i.e. after swapchain recreation).
     */
    class DeferredLightPipelinePass : public PipelinePass
    {
    public:
        explicit DeferredLightPipelinePass(Pipeline::Stage stage, LightManager &lightManager);
        ~DeferredLightPipelinePass() override = default;

        void Render(const CommandBuffer &commandBuffer) override;
        void RefreshGBufferDescriptors();

    private:
        LightManager &lm_;
        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;

        const Image2d *lastAlbedo_ = nullptr;
        const Image2d *lastNormal_ = nullptr;
        const Image2d *lastPbr_ = nullptr;
        const ImageDepth *lastDepth_ = nullptr;
    };
}
