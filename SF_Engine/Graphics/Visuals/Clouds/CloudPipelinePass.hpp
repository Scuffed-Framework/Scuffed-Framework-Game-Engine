#pragma once

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Buffers/UniformBuffer.hpp>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Commands/CommandBuffer.hpp>
#include <Graphics/Visuals/Clouds/LUT/CloudNoiseLUTs.hpp>

#include <glm/glm.hpp>
#include <memory>
#include <vulkan/vulkan.h>

namespace SF::Engine
{
    // -------------------------------------------------------------------------
    // CloudFrameUBO
    // Uploaded once per frame at binding 5 of the raymarch descriptor set.
    // std140 layout -- every field is explicitly padded to 16-byte boundaries.
    // MUST match the uniform block declared in CloudRaymarch.shader exactly.
    // -------------------------------------------------------------------------
    struct CloudFrameUBO
    {
        glm::mat4 invViewProj = glm::mat4(1.0f); // clip -> world

        glm::vec3 cameraPos = glm::vec3(0.0f);
        float time = 0.0f; // elapsed seconds

        glm::vec3 sunDir = glm::vec3(0.577f, 0.577f, 0.577f);
        float sunIntensity = 40.0f;

        glm::vec3 sunColor = glm::vec3(1.0f, 0.95f, 0.85f);
        float cloudSpeed = 0.02f; // horizontal wind speed

        float cloudBot = 1600.0f;       // cloud layer bottom (metres)
        float cloudTop = 2100.0f;       // cloud layer top    (metres)
        float cloudDensity = 0.15f;     // optical depth scale
        float earthRadius = 6371000.0f; // metres

        glm::vec2 screenSize = glm::vec2(1280.0f, 720.0f);
        float windAngle = 0.0f; // wind direction (radians)
        float _pad0 = 0.0f;

        glm::vec2 windOffset = glm::vec2(0.0f); // accumulated wind displacement (metres XZ)
        glm::vec2 _pad1 = glm::vec2(0.0f);
    };

    // -------------------------------------------------------------------------
    // CloudPipelinePass
    //
    // Two-pass volumetric cloud renderer:
    //   PreRender()  -- half-resolution offscreen raymarch  (CloudRaymarch.shader)
    //   Render()     -- fullscreen composite over scene     (CloudComposite.shader)
    //
    // Setup:
    //   auto *clouds = manager->Add<CloudPipelinePass>(
    //       stage, std::make_unique<CloudPipelinePass>(stage));
    //
    // Per-frame (before PreRender):
    //   clouds->SetFrameData(invViewProj, camPos, sunDir, deltaTime);
    //   clouds->OnResize(width, height);   // only on viewport change
    // -------------------------------------------------------------------------
    class CloudPipelinePass : public PipelinePass
    {
    public:
        explicit CloudPipelinePass(Pipeline::Stage compositeStage);
        ~CloudPipelinePass() override;

        // Call every frame before PreRender().
        void SetFrameData(const glm::mat4 &invViewProj,
                          const glm::vec3 &cameraPos,
                          const glm::vec3 &sunDir,
                          float deltaTime);

        // Call when the swapchain is resized.
        void OnResize(uint32_t fullWidth, uint32_t fullHeight);

        // PipelinePass interface
        void PreRender(const CommandBuffer &cmd) override;
        void Render(const CommandBuffer &cmd) override;

        // Expose the cloud buffer for downstream passes (e.g. god-rays).
        Image2d *GetCloudBuffer() const { return cloudBuffer_.get(); }

    private:
        // Helpers
        void destroyOffscreenVkObjects();
        void createOffscreenBuffer(uint32_t w, uint32_t h);
        void createRaymarchPipeline();
        void createCompositePipeline();
        void writeRaymarchDescriptors();
        void writeCompositeDescriptors();

        // ---- LUTs (baked once at construction) ---------------------------
        std::unique_ptr<WorleyNoiseLUT3D> basicNoise_;
        std::unique_ptr<CurlNoiseLUT3D> detailNoise_;
        std::unique_ptr<CoverageLUT> coverage_;
        std::unique_ptr<BlueNoiseLUT> blueNoise_;
        std::unique_ptr<ShadowLUT> shadowLUT_;

        // ---- Per-frame UBO -----------------------------------------------
        std::unique_ptr<UniformBuffer> ubo_;
        CloudFrameUBO uboData_{};

        // Wind velocity accumulated into uboData_.windOffset each frame (XZ metres/sec)
        glm::vec2 windVelocity = glm::vec2(1.0f, 0.0f);

        // ---- Offscreen raymarch buffer (half-res) -------------------------
        std::unique_ptr<Image2d> cloudBuffer_;
        VkRenderPass offscreenRP_ = VK_NULL_HANDLE;
        VkFramebuffer offscreenFB_ = VK_NULL_HANDLE;
        uint32_t offW_ = 0;
        uint32_t offH_ = 0;

        // ---- Raymarch pipeline (writes to offscreen buffer) --------------
        std::unique_ptr<RenderPipeline> raymarchPipeline_;
        std::unique_ptr<DescriptorSet> raymarchDescSet_;

        // ---- Composite pipeline (reads cloud buffer, writes swapchain) ---
        std::unique_ptr<RenderPipeline> compositePipeline_;
        std::unique_ptr<DescriptorSet> compositeDescSet_;

        // Cached logical device handle (not owned here)
        VkDevice device_ = VK_NULL_HANDLE;
    };

} // namespace SF::Engine
