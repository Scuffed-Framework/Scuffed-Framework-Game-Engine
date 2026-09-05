#pragma once

#include <Rendering/Buffers/UniformBuffer.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <Rendering/PipelinePassManager.hpp>
#include <Rendering/Pipelines/ComputePipeline.hpp>
#include "LUT/MultiScatterLUT.hpp"
#include "LUT/SkyViewLUT.hpp"
#include "LUT/TransmittanceLUT.hpp"

#include <Math/BasicMath.hpp>
#include <memory>
#include "../../Water/OceanPipelinePass.hpp"

#include "LUT/AtmoLUTs.hpp"

namespace SF::Engine
{
    // Composites the sky / aerial-perspective result directly into the "hdr"
    // scene-colour image via a compute dispatch (RWTexture2D read-modify-write),
    // rather than drawing a fullscreen triangle into a separate attachment.
    class AtmospherePipelinePass : public PipelinePass
    {
    public:
        explicit AtmospherePipelinePass(Pipeline::Stage stage, const AtmosphereParams &params = {});
        ~AtmospherePipelinePass() override = default;

        void PreRender(const CommandBuffer &commandBuffer) override;

        // Transitions "hdr" to VK_IMAGE_LAYOUT_GENERAL, dispatches the compute
        // shader, then transitions it back for whatever consumes it next.
        void Render(const CommandBuffer &commandBuffer) override;

        void SetSceneBuffers();
        void SetFrameData(const Mat4 &invProj, const Mat4 &invView, const Vec3 &cameraPos, const Vec3 &planetPos,
                          const Vec3 &sunDir, Vec2 screenSize);

        void SetParams(const AtmosphereParams &params) { params_ = params; }

        AtmosphereParams &GetParams() { return params_; }
        [[nodiscard]] const AtmosphereParams &GetParams() const { return params_; }

    private:
        AtmosphereParams params_;

        std::unique_ptr<ComputePipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;

        AtmosphereFrameUBO frameData_{};

        // "hdr" is now both the compute target (binding 8, storage image) and
        // the thing we track for change detection, so lastColor_ doubles as
        // "which image we last wrote descriptor binding 8 for".
        const Image2d *lastColor_    = nullptr;
        const ImageDepth *lastDepth_ = nullptr;
    };
} // namespace SF::Engine
