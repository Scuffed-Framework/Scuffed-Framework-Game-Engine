#pragma once

#include <Rendering/PipelinePassManager.hpp>
#include <Rendering/Pipelines/ComputePipeline.hpp>
#include <Rendering/Buffers/UniformBuffer.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <Rendering/Images/Image2d.hpp>
#include <Rendering/Images/ImageDepth.hpp>
#include <Rendering/Lighting/LightManager.hpp>
#include <Gui/UIRegistry.hpp>

#include <Math/BasicMath.hpp>
#include <memory>
#include <cstdint>

namespace SF::Engine
{
    // Matches SSR_CAMERA_BIND in Shaders/SSR/SSRCommon.si, and the literal
    // binding (31) CloudPipelinePass already uses for the same shared
    // Camera UBO — kept as a named constant here instead of a repeated
    // magic number in SSRPipelinePass.cpp.
    static constexpr uint32_t kSSRCameraBind = 31;

    // Mirrors Shaders/SSR/SSRCommon.si's SSRParams exactly — 7 x 16-byte rows,
    // std140 layout. Keep the two definitions in lockstep; the static_assert
    // below only catches size drift, not field-order drift.
    struct alignas(16) SSRParams
    {
        Vec2 screenSize;
        Vec2 invScreenSize;

        int32_t maxSteps;
        float thickness;
        float strideScale;
        int32_t frameIndex;

        float maxRoughness;
        float intensity;
        float temporalBlendMin;
        float temporalBlendMax;

        float spatialRadiusPx;
        float varianceClampGamma;
        int32_t debugView;
        int32_t binarySearchSteps;

        Vec3 ambientSkyColor;
        float ambientIntensity;

        Vec3 ambientGroundColor;
        float edgeFadeStart;

        int32_t bTemporalEnabled;
        int32_t bSpatialEnabled;
        int32_t bProbeFallbackEnabled;
        float depthBufferThicknessBias;
    };

    static_assert(sizeof(SSRParams) == 112, "SSRParams size mismatch - check cpu/gpu side");
    static_assert(sizeof(SSRParams) % 16 == 0, "SSRParams must satisfy std140 alignment");

    // Debug view modes — must match SSR_DEBUG_* in Shaders/SSR/SSRCommon.si.
    enum class SSRDebugView : int32_t
    {
        None = 0,
        RayDir = 1,
        TraceRaw = 2,
        Temporal = 3,
        Spatial = 4,
        Confidence = 5,
    };

    /**
     * @brief Probed Stochastic Screen-Space Reflections.
     *
     * Five-stage compute pipeline, each stage its own dispatch so any of
     * them can be toggled or inspected independently (see debugView and
     * the bXxxEnabled flags below):
     *
     *   GBuffer/hdr -> RayGen -> Trace (+ probe fallback on miss)
     *                -> TemporalAccumulate -> SpatialFilter -> Composite
     *
     * Runs entirely in PreRender() (Render() is a no-op), following the
     * same cross-stage-attachment pattern as CloudPipelinePass: it reads
     * "gbuf_depth"/"gbuf_normal"/"gbuf_albedo"/"gbuf_pbr" (fully up to date
     * for this frame, since GBuffer stage 0 has already completed by the
     * time stage 1's PreRender runs) and "hdr" (one frame stale at the
     * point PreRender reads it, since all of stage 1's PreRender calls run
     * before stage 1's renderpass — i.e. before DeferredLightPipelinePass
     * writes this frame's lit color into hdr). That one-frame lag on the
     * scene-colour input is an accepted trade-off already present in
     * Clouds' compositing and is generally imperceptible once temporal
     * accumulation is smoothing the result anyway.
     *
     * Register at Pipeline::Stage{1, 1} in LightingRenderer, after the
     * deferred-light subpass and before forward-transparent — see
     * LightingRenderer.hpp.
     */
    class SSRPipelinePass : public PipelinePass
    {
    public:
        explicit SSRPipelinePass(Pipeline::Stage stage, LightManager &lightManager);
        ~SSRPipelinePass() override = default;

        void PreRender(const CommandBuffer &cmd) override;
        void Render(const CommandBuffer &cmd) override {}

        void DrawImGuiPanel();

        static bool isWindowOpen;

        // --- Stage toggles : each corresponds to a box in the architecture
        // diagram and can be flipped independently for debugging. ---
        bool enabled = true;
        bool temporalEnabled = true;
        bool spatialEnabled = true;
        bool probeFallbackEnabled = true;

        // --- Tunables (mirrors SSRParams; kept here as the ImGui-editable
        // source of truth, written into ssrUBO_ every frame in PreRender). ---
        int maxSteps = 32;
        float thickness = 0.35f;       // view-space units
        float strideScale = 1.0f;
        float maxRoughness = 0.85f;
        float intensity = 1.0f;
        float temporalBlendMin = 0.03f;
        float temporalBlendMax = 1.0f;
        float spatialRadiusPx = 8.0f;
        float varianceClampGamma = 4.0f;
        int binarySearchSteps = 6;
        float edgeFadeStart = 0.1f;
        float depthBufferThicknessBias = 0.02f;
        Vec3 ambientSkyColor = {0.45f, 0.6f, 0.9f};
        Vec3 ambientGroundColor = {0.2f, 0.18f, 0.15f};
        float ambientIntensity = 0.6f;
        SSRDebugView debugView = SSRDebugView::None;

    private:
        void CreateResources();
        void CreatePipelines();
        void BindStaticDescriptors();
        void UpdateUBO();

        LightManager &lm_;

        std::unique_ptr<UniformBuffer> ssrUBO_;

        std::unique_ptr<ComputePipeline> rayGenPipeline_;
        std::unique_ptr<ComputePipeline> tracePipeline_;
        std::unique_ptr<ComputePipeline> temporalPipeline_;
        std::unique_ptr<ComputePipeline> spatialPipeline_;
        std::unique_ptr<ComputePipeline> compositePipeline_;

        static constexpr uint32_t kFramesInFlight = 3;

        // Intra-frame scratch : written and consumed within the same
        // PreRender() call, so — like CloudPipelinePass's cloudRenderRT_/
        // cloudDepthRT_/cloudFogRT_ — these do NOT need per-frame-in-flight
        // ping-ponging; the pipeline barriers inserted between dispatches
        // already provide correct ordering across frames on a single queue.
        std::unique_ptr<Image2d> rayDirRT_;    // rgb=dir WS, a=NdotH
        std::unique_ptr<Image2d> rayDataRT_;   // r=roughnessA g=metallic b=skyMask a=pdf
        std::unique_ptr<Image2d> traceColorRT_;// rgb=radiance, a=confidence
        std::unique_ptr<Image2d> traceHitRT_;  // r=hitMask g=hitT b=pdf
        std::unique_ptr<Image2d> filteredRT_;  // rgb=denoised, a=confidence

        // Carries state across frames -> must be ping-ponged (same
        // reasoning as CloudPipelinePass's reconColor_/reconDepth_/reconFog_).
        std::unique_ptr<Image2d> accumColor_[kFramesInFlight];   // rgb + a=confidence
        std::unique_ptr<Image2d> accumMoments_[kFramesInFlight]; // r=mean g=mean2 b=historyCount a=hitMask

        std::unique_ptr<DescriptorSet> rayGenSet_;
        std::unique_ptr<DescriptorSet> traceSet_;
        std::unique_ptr<DescriptorSet> temporalSet_[kFramesInFlight];
        std::unique_ptr<DescriptorSet> spatialSet_[kFramesInFlight];
        std::unique_ptr<DescriptorSet> compositeSet_[kFramesInFlight];

        std::unique_ptr<Image2d> dummyTexture_; // frame-0 history fallback (RGBA16F, 1x1)

        uint32_t frameSlot_ = 0;
        uint32_t framesSinceStart_ = 0;
        uint32_t frameCounter_ = 0;

        std::size_t uiHandle_ = 0;
    };
} // namespace SF::Engine
