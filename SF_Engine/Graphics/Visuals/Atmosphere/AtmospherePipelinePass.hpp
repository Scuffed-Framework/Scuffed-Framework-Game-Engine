#pragma once

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Buffers/UniformBuffer.hpp>
#include "LUT/TransmittanceLUT.hpp"
#include "LUT/MultiScatterLUT.hpp"
#include "AtmosphereParams.hpp"

#include <glm/glm.hpp>
#include <memory>

namespace SF::Engine
{
    class AtmospherePipelinePass : public PipelinePass
    {
    public:
        explicit AtmospherePipelinePass(Pipeline::Stage stage,
                                        const AtmosphereParams &params = {});
        ~AtmospherePipelinePass() override = default;

        void Render(const CommandBuffer &commandBuffer) override;

        void SetFrameData(const glm::mat4 &invProj,
                          const glm::mat4 &invView,
                          const glm::vec3 &cameraPos,
                          const glm::vec3 &planetPos,
                          const glm::vec3 &sunDir,
                          glm::vec2 screenSize);

        void SetParams(const AtmosphereParams &params) { params_ = params; }

        AtmosphereParams &GetParams() { return params_; }
        const AtmosphereParams &GetParams() const { return params_; }

        TransmittanceLUT *GetTransmittanceLUT() const { return transmittanceLUT_.get(); }
        MultiScatterLUT *GetMultiScatterLUT() const { return multiScatterLUT_.get(); }

    private:
        AtmosphereParams params_;

        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;

        std::unique_ptr<TransmittanceLUT> transmittanceLUT_;
        std::unique_ptr<MultiScatterLUT> multiScatterLUT_;

        AtmosphereFrameUBO frameData_{};
    };
}
