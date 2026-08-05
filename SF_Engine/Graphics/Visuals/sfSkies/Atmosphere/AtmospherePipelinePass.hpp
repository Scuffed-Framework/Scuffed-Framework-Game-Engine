#pragma once

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Buffers/UniformBuffer.hpp>
#include "LUT/TransmittanceLUT.hpp"
#include "LUT/MultiScatterLUT.hpp"
#include "LUT/SkyViewLUT.hpp"

#include <Math/BasicMath.hpp>
#include <memory>
#include "../../Water/OceanPipelinePass.hpp"

#include "LUT/AtmoLUTs.hpp"

namespace SF::Engine
{
    class AtmospherePipelinePass : public PipelinePass
    {
    public:
        explicit AtmospherePipelinePass(Pipeline::Stage stage,
                                        const AtmosphereParams &params = {});
        ~AtmospherePipelinePass() override = default;

        void PreRender(const CommandBuffer &commandBuffer);

        void Render(const CommandBuffer &commandBuffer) override;

        void SetSceneBuffers();
        void SetFrameData(const Mat4 &invProj,
                          const Mat4 &invView,
                          const Vec3 &cameraPos,
                          const Vec3 &planetPos,
                          const Vec3 &sunDir,
                          glm::vec2 screenSize);

        void SetParams(const AtmosphereParams &params) { params_ = params; }

        AtmosphereParams &GetParams() { return params_; }
        const AtmosphereParams &GetParams() const { return params_; }

        const AtmoLUTs *GetAtmosphereSharedLUTs() const { return atmoLUTs_.get(); }
        AtmoLUTs *GetAtmosphereSharedLUTs() { return atmoLUTs_.get(); }

    private:
        AtmosphereParams params_;

        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;

        AtmosphereFrameUBO frameData_{};
        std::unique_ptr<AtmoLUTs> atmoLUTs_;

        const Image2d *lastColor_ = nullptr;
        const ImageDepth *lastDepth_ = nullptr;
    };
}
