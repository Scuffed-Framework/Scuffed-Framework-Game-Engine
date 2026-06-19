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

#include <glm/glm.hpp>
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
        float windSpeed;         // noise-scroll speed multiplier
        float cloudType;         // 0 = billowy, 1 = wispy (global bias; NVDF overrides)

        float time; // accumulated seconds (wind animation)
        float _pad0;

        float sdfRangeMetres; // max SDF distance in G channel

        float _pad1;
    };
    static_assert(sizeof(CloudUBO) == 48, "CloudUBO size mismatch - check padding");
    static_assert(sizeof(CloudUBO) % 16 == 0, "CloudUBO must satisfy std140 alignment");

    class CloudPipelinePass : public PipelinePass
    {
    public:
        explicit CloudPipelinePass(Pipeline::Stage stage,
                                   const AtmosphereParams &params);
        ~CloudPipelinePass() override = default;

        void SetFrameData(const glm::mat4 &invProj,
                          const glm::mat4 &invView,
                          const glm::vec3 &cameraPos,
                          const glm::vec3 &planetPos,
                          const glm::vec3 &sunDir,
                          glm::vec2 screenSize);

        void PreRender(const CommandBuffer &cmd);

        void Render(const CommandBuffer &cmd) override;

        void DrawImGuiPanel();

        bool enabled = true;
        float minAlt = 1500.0f; // metres above planet surface
        float maxAlt = 6000.0f;

        int marchSteps = 96;      // primary ray
        int lightMarchSteps = 24; // secondary (light accum compute)

        float densityScale = 1.0f;
        float coverage = 0.50f;

        float extinctionCoeff = 0.06f;  // σ_e (m^-1)
        float scatteringAlbedo = 0.90f; // σ_s / σ_e

        float windSpeed = 1.0f;

        float cloudType = 0.0f; // 0 = cumulus, 1 = cirrus

        float cloudletRadius = 2000.0f; // base radius (m)
        int cloudletCount = 12;

        void RebuildCloudlets(); // regenerates and re-bakes the NVDF

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
        std::unique_ptr<TransmittanceLUT> transmittanceLUT_;
        std::unique_ptr<MultiScatterLUT> multiScatterLUT_;

        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;

        glm::vec3 cachedSunDir_{0.577f, 0.577f, 0.577f};
        float totalTime_{0.0f};
    };

} // namespace SF::Engine
