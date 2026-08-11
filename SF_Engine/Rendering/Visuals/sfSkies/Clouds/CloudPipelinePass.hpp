#pragma once

#include <Rendering/PipelinePassManager.hpp>
#include <Rendering/Visuals/sfSkies/Atmosphere/AtmosphereParams.hpp>
#include <Rendering/Pipelines/RenderPipeline.hpp>
#include <Rendering/Buffers/UniformBuffer.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <Rendering/LUT/BlueNoiseLUT.hpp>
#include <Rendering/LUT/AlligatorNoiseLUT.hpp>
#include "../Atmosphere/LUT/TransmittanceLUT.hpp"
#include "../Atmosphere/LUT/MultiScatterLUT.hpp"
#include "CloudNoise.hpp"
#include "../Atmosphere/LUT/AerialPerspectiveLUT.hpp"
#include <Rendering/Pipelines/ComputePipeline.hpp>
#include <Gui/UIRegistry.hpp>

#include <Math/BasicMath.hpp>
#include <memory>
#include <cstdint>

namespace SF::Engine
{
    struct alignas(16) CloudUBO
    {
        float cloudBottomRadius;
        float cloudTopRadius;
        float stepCount;
        float lightStepCount;

        float cloudDensityScale;
        float cloudCoverage;

        float time;

        float sdfRangeMetres;
        int frameIndex;

        float cloudDetailScale;
        float cloudBaseNoiseScale;
        float cloudCurlNoiseScale;

        float cloudWeatherUVScale;
        float percipitationBias;
        float FadeDistance2d;
        float fadeSmoothDist;

        Vec2 Wind;
        float Speed;
        float unused;
    };

    static_assert(sizeof(CloudUBO) == 80, "CloudUBO size mismatch - check cpu/gpu side");
    static_assert(sizeof(CloudUBO) % 16 == 0, "CloudUBO must satisfy std140 alignment");

    class CloudPipelinePass : public PipelinePass
    {
    public:
        explicit CloudPipelinePass(Pipeline::Stage stage,
                                   AtmosphereData &data);
        ~CloudPipelinePass() override = default;

        void SetFrameData(const Mat4 &invProj,
                          const Mat4 &invView,
                          const Vec3 &cameraPos,
                          const Vec3 &planetPos,
                          const Vec3 &sunDir,
                          Vec2 screenSize);

        void PreRender(const CommandBuffer &cmd);

        void Render(const CommandBuffer &cmd) override;

        void DrawImGuiPanel();

        bool enabled = true;
        float minAlt = 1500.0f; // metres above planet surface
        float maxAlt = 6000.0f;

        int marchSteps = 32;     // primary ray
        int lightMarchSteps = 8; // secondary (light accum compute)

        float densityScale = 1.0f;
        float coverage = 0.5;
        float cloudDetailScale = 0.00025f;
        float cloudBaseNoiseScale = 1.0f;
        float cloudCurlNoiseScale = 1.0f;

        float cloudWeatherUVScale = 0.500f;
        float percipitationBias = 1.0f;
        float FadeDistance2d = 1.0f;
        float fadeSmoothDist = 1.0f;
        float time;
        float sdfRangeMetres;
        Vec2 Wind = Vec2{0,0};
        float Speed = 0.0f;
        float unused = 0.0f;

        static bool isWindowOpen;

    private:
        void BindDescriptors();
        void UpdateCloudUBO();
        void PopulateDefaultCloudlets();

        AtmosphereData &data_;
        std::unique_ptr<UniformBuffer> atmoUBO_;  // binding=0
        std::unique_ptr<UniformBuffer> cloudUBO_; // binding=1

        std::unique_ptr<CloudNoiseLUTs> cloudNoise_;

        std::unique_ptr<ComputePipeline> raymarchPipeline_;
        std::unique_ptr<ComputePipeline> reconstructPipeline_;
        std::unique_ptr<ComputePipeline> compositePipeline_;
        std::unique_ptr<ComputePipeline> lensBufferPipeline_; // unused for now, per your note

        std::unique_ptr<DescriptorSet> raymarchSet_; // written once in BindDescriptors(), never re-written per frame

        // Rewritten every frame in PreRender() (composite's binding 0/4, and the
        // ping-ponged reconstruct/composite bindings) so they need one slot per
        // frame-in-flight. The engine keeps 3 frames in flight, so 3 slots —
        // matching reconColor_/reconDepth_/reconFog_ below. Index with
        // frameSlot_ ("cur"), same as the ping-pong images.
        static constexpr uint32_t kFramesInFlight = 3;
        std::unique_ptr<DescriptorSet> reconstructSet_[kFramesInFlight];
        std::unique_ptr<DescriptorSet> compositeSet_[kFramesInFlight];
        std::unique_ptr<DescriptorSet> lensBufferSet;

        Vec3 cachedSunDir_{0.577f, 0.577f, 0.577f};
        float totalTime_{0.0f};

        const Image2d *lastColorImg_ = nullptr;
        const ImageDepth *lastDepthImg_ = nullptr;

        uint64_t lastAttachmentGeneration_ = 0;
        uint32_t frameCounter_ = 0;

        std::unique_ptr<Image2d> cloudRenderRT_; // binding 2/3  rgba16f
        std::unique_ptr<Image2d> cloudDepthRT_;  // binding 14/15 r32f
        std::unique_ptr<Image2d> cloudFogRT_;    // binding 22/23 rgba16f

        // Full-res reconstruction targets, cycled across kFramesInFlight slots for
        // both temporal history and to avoid CPU-mutating a slot the GPU still has
        // in flight.
        // index [frameSlot_]      = "current" (written this frame, read by composite)
        // index [frameSlot_ - 1]  = "history" (previous frame's current, read by reconstruct)
        std::unique_ptr<Image2d> reconColor_[kFramesInFlight]; // binding 12(write)/13(read)/18(history)
        std::unique_ptr<Image2d> reconDepth_[kFramesInFlight]; // binding 16(write)/17(read)/19(history)
        std::unique_ptr<Image2d> reconFog_[kFramesInFlight];   // binding 24(write)/25(read)/26(history)

        uint32_t framesSinceStart_ = 0;
        uint32_t frameSlot_ = 0; // monotonically increasing, index with % kFramesInFlight
        bool firstFrame_ = true;
        // fahh
        std::unique_ptr<Image2d> dummyTexture_;
        std::size_t uiHandle;
    };

} // namespace SF::Engine