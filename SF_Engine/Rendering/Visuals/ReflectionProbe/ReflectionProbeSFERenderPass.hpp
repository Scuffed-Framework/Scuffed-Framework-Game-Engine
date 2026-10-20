#pragma once

#include <memory>
#include <vector>

#include <Math/Vectors/Vector.hpp>
#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>
#include <Rendering/RHI/Descriptors/DescriptorSet.hpp>
#include <Rendering/RHI/Images/Image.hpp>
#include <Rendering/RHI/Memory/UniformBuffer.hpp>
#include <Rendering/RHI/Pipelines/ComputePipeline.hpp>
#include <Rendering/Visuals/sfSkies/Atmosphere/AtmosphereSFERenderPass.hpp>

namespace SF::Engine
{
    /**
     * @brief Global reflection probe: one prefiltered RGBA16F cubemap holding the environment, used by
     * SSR (Trace.shader) wherever the screen-space trace has nothing to say -- ray misses, rays that
     * leave the screen, and surfaces rough enough that tracing is skipped entirely.
     *
     * Contents today: the sky, captured from the atmosphere's SkyView LUT (or the analytic
     * sky/ground gradient when no atmosphere exists). Capturing actual scene geometry into the cube
     * is the planned next step; the cube, prefilter chain and the sampling side are already
     * written for it (mip 0 = raw radiance, mips 1..5 = GGX-prefiltered, see ReflectionProbe.si).
     *
     * Pure compute, stage-less like HiZSFERenderPass/ClusterCullSFERenderPass: everything happens in
     * PreRender(), Render() is a no-op. Must run AFTER the atmosphere's PreRender (it reads the
     * SkyView LUT that pass bakes) and BEFORE SSR's -- SceneRenderer registers it at Stage{1,0}
     * with a high order, ahead of SSR at Stage{1,1}.
     *
     * The cube lives permanently in VK_IMAGE_LAYOUT_GENERAL (storage write + sampled read, no layout
     * transitions); consumers bind GetCubemap()->GetView() as a sampled cube image in GENERAL.
     */
    class ReflectionProbeSFERenderPass : public EngineRenderpass
    {
    public:
        explicit ReflectionProbeSFERenderPass(Pipeline::Stage stage);
        ~ReflectionProbeSFERenderPass() override;

        void PreRender(const CommandBuffer &commandBuffer) override;
        void Render(const CommandBuffer &) override {}

        // Atmosphere to capture the sky from, or nullptr to use the analytic gradient. Set every
        // frame by SceneRenderer (the atmosphere is created/destroyed at runtime by its controller).
        void SetAtmosphere(const AtmosphereSFERenderPass *atmosphere) { atmosphere_ = atmosphere; }

        // Analytic sky/ground used when there is no atmosphere (rgb * intensity).
        void SetAnalyticSky(const Vec3 &sky, float intensity, const Vec3 &ground)
        {
            analyticSky_       = sky;
            analyticIntensity_ = intensity;
            analyticGround_    = ground;
        }

        // Recapture every N frames (1 = every frame). The sky changes slowly; the first capture
        // always happens immediately.
        void SetUpdateInterval(uint32_t frames) { updateInterval_ = frames == 0 ? 1 : frames; }

        [[nodiscard]] Cubemap *GetCubemap() const { return cube_.get(); }
        // Bumped whenever the cube image is (re)created; consumers rewrite their descriptor then.
        [[nodiscard]] uint64_t GetGeneration() const { return generation_; }

        // Face size and number of used mips. kMipCount - 1 MUST equal kProbeMaxMip in
        // Shaders/ReflectionProbe/ReflectionProbe.si.
        static constexpr uint32_t kFaceSize = 128;
        static constexpr uint32_t kMipCount = 6; // 128, 64, 32, 16, 8, 4

    private:
        void CreateCube();
        void DestroyViews();

        std::unique_ptr<ComputePipeline> skyPipeline_;
        std::unique_ptr<ComputePipeline> prefilterPipeline_;
        std::unique_ptr<DescriptorSet> skySet_;
        std::vector<std::unique_ptr<DescriptorSet>> prefilterSets_; // index m-1 writes mip m

        std::unique_ptr<UniformBuffer> atmoUbo_;   // AtmoUBO-layout copy for ProbeSky.shader
        std::unique_ptr<UniformBuffer> paramsUbo_; // ProbeParams for ProbeSky.shader

        std::unique_ptr<Cubemap> cube_;
        // Per-mip 2D-array storage views (6 layers) and a single-mip cube view of mip 0 for the
        // prefilter to sample (Cubemap only exposes one whole-chain cube view).
        std::vector<VkImageView> mipViews_;
        VkImageView srcView_ = VK_NULL_HANDLE;

        VkDevice device_ = VK_NULL_HANDLE;

        const AtmosphereSFERenderPass *atmosphere_ = nullptr;
        Vec3 analyticSky_{0.45f, 0.6f, 0.9f};
        Vec3 analyticGround_{0.2f, 0.18f, 0.15f};
        float analyticIntensity_ = 0.6f;

        uint32_t levelCount_     = 0;
        uint32_t updateInterval_ = 1;
        uint32_t frameCounter_   = 0;
        bool captured_           = false;
        uint64_t generation_     = 0;
    };
} // namespace SF::Engine
