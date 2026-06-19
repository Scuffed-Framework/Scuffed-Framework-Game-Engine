#pragma once

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Buffers/UniformBuffer.hpp>
#include <Graphics/Visuals/sfSkies/Atmosphere/LUT/TransmittanceLUT.hpp>
#include "SunParams.hpp"

#include <glm/glm.hpp>
#include <memory>

namespace SF::Engine
{
    /**
     * @brief Renders a physically-tinted sun disc + halo at the directional light position.
     *
     * Standalone : works with or without AtmospherePipelinePass.
     * Rendered at sky depth (z=0.9999) so it appears behind all geometry.
     * The disc uses the same Rayleigh/Mie transmittance march as Atmosphere.shader
     * so its colour matches the sky exactly at the horizon.
     *
     * Usage:
     *   sunPass_ = AddPipelinePass<SunPipelinePass>(stage, SunParams{});
     *
     *   // Each frame:
     *   sunPass_->SetFrameData(invProj, invView, sunDir, screenSize);
     *
     *   // Optionally tweak params at runtime:
     *   sunPass_->GetParams().intensity = 15.0f;
     */
    class SunPipelinePass : public PipelinePass
    {
    public:
        explicit SunPipelinePass(Pipeline::Stage stage, SunParams params = SunParams{});
        ~SunPipelinePass() override = default;

        void Render(const CommandBuffer &commandBuffer) override;

        /**
         * @brief Call every frame before Render().
         * @param invProj  Inverse projection (Vulkan Y-flipped).
         * @param invView  Inverse view matrix.
         * @param sunDir   World-space unit vector pointing TOWARD the sun.
         * @param screenSize Viewport size in pixels.
         */
        void SetFrameData(const glm::mat4 &invProj,
                          const glm::mat4 &invView,
                          const glm::vec3 &sunDir,
                          glm::vec2 screenSize);

        SunParams &GetParams() { return params_; }
        const SunParams &GetParams() const { return params_; }

    private:
        void WriteDescriptors();

        static VkWriteDescriptorSet WUbo(VkDescriptorSet d, uint32_t b,
                                         const VkDescriptorBufferInfo *i);

        SunParams params_;
        SunUBO uboData_{};

        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;
        std::unique_ptr<TransmittanceLUT> transmittanceLUT_;
    };
}
