#pragma once

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Visuals/sfSkies/Atmosphere/AtmosphereParams.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Buffers/UniformBuffer.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/LUT/BlueNoiseLUT.hpp>
#include <Graphics/LUT/AlligatorNoiseLUT.hpp>
#include "../Atmosphere/LUT/TransmittanceLUT.hpp"
#include "../Atmosphere/LUT/MultiScatterLUT.hpp"
#include "CloudNoise.hpp"
#include "../Atmosphere/LUT/AerialPerspectiveLUT.hpp"

#include <Math/BasicMath.hpp>
#include <memory>
#include <cstdint>

namespace SF::Engine
{
    struct alignas(16) CloudUBO
    {
        float cloudBottomRadius; // params.bottomRadius + minAlt
        float cloudTopRadius;    // params.bottomRadius + maxAlt
        float stepCount;         // primary ray step budget  (float for shader compat)
        float lightStepCount;    // kept for ABI; unused in shader (precomputed accum)

        float cloudDensityScale; // global density multiplier
        float cloudCoverage;     // [0, 1] coverage bias

        float time; // accumulated seconds (wind animation)

        float sdfRangeMetres; // max SDF distance in G channel
        int frameIndex;       // frame counter for blue noise sampling
        
        float cloudDetailScale;
        float cloudBaseNoiseScale;
        float cloudCurlNoiseScale;

        float cloudWeatherUVScale;
        float percipitationBias;
        float FadeDistance2d;
        float fadeSmoothDist;
    };

    static_assert(sizeof(CloudUBO) == 64, "CloudUBO size mismatch - check padding");
    static_assert(sizeof(CloudUBO) % 16 == 0, "CloudUBO must satisfy std140 alignment");

    class CloudPipelinePass : public PipelinePass
    {
    public:
        explicit CloudPipelinePass(Pipeline::Stage stage,
                                   const AtmosphereParams &params);
        ~CloudPipelinePass() override = default;

        void SetFrameData(const Mat4 &invProj,
                          const Mat4 &invView,
                          const Vec3 &cameraPos,
                          const Vec3 &planetPos,
                          const Vec3 &sunDir,
                          glm::vec2 screenSize);

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
        float cloudDetailScale = 1.0f;
        float cloudBaseNoiseScale = 1.0f;
        float cloudCurlNoiseScale = 1.0f;

        float cloudWeatherUVScale = 1.0f;
        float percipitationBias = 1.0f;
        float FadeDistance2d = 1.0f;
        float fadeSmoothDist = 1.0f;

        static bool isWindowOpen;

    private:
        void BindDescriptors();
        void UpdateCloudUBO();
        void PopulateDefaultCloudlets();

        const AtmosphereParams &params_;

        AtmosphereFrameUBO frameData_{};
        std::unique_ptr<UniformBuffer> atmoUBO_;  // binding=0
        std::unique_ptr<UniformBuffer> cloudUBO_; // binding=1

        std::unique_ptr<BlueNoiseLUT> blueNoiseLUT_;
        std::unique_ptr<PerlinWorleyNoiseLUT> pWorleyLUT_;
        std::unique_ptr<CloudNoiseLUTs> cloudNoise_;
        std::unique_ptr<AerialPerspectiveLUT> aerialPerspRange_;

        std::unique_ptr<TransmittanceLUT> transmittanceLUT_;
        std::unique_ptr<MultiScatterLUT> multiScatterLUT_;

        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;

        Vec3 cachedSunDir_{0.577f, 0.577f, 0.577f};
        float totalTime_{0.0f};

        const Image2d *lastColorImg_ = nullptr;
        const ImageDepth *lastDepthImg_ = nullptr;

        uint64_t lastAttachmentGeneration_ = 0;
        uint32_t frameCounter_ = 0;
    };

} // namespace SF::Engine
