#pragma once

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Pipelines/ComputePipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Buffers/UniformBuffer.hpp>
#include <Graphics/PipelinePassInit.hpp>
#include <Graphics/Images/Image2dArray.hpp>
#include "OceanTessellatedMesh.hpp"
#include "OceanClipmapMesh.hpp"
#include <memory>
#include "OceanFFTSpectrum.hpp"

namespace SF::Engine
{
    // TODO: fix lol
    struct OceanTessellationParams
    {
        //  Gerstner waves
        float waveAmplitude = 1.2f;
        float waveFrequency = 0.25f; // wavenumber k (cycles / world unit)
        float waveSpeed = 1.2f;      // phase speed
        float waveSteepness = 0.7f;  // Q in [0, 1]; controls crest sharpness

        float waveAmplitude2 = 0.5f;
        float waveFrequency2 = 0.6f;
        float waveSpeed2 = 1.8f;

        //  Visual
        glm::vec3 oceanColor = glm::vec3(0.02f, 0.05f, 0.12f);
        float glossiness = 0.85f;
        glm::vec3 shallowColor = glm::vec3(0.10f, 0.35f, 0.40f);
        float specularPower = 96.0f;
        glm::vec3 foamColor = glm::vec3(0.85f, 0.92f, 0.95f);
        float foamIntensity = 0.60f;
        float foamThreshold = 0.15f;

        //  Tessellation
        float tessFactor = 64.0f;        // max tess level (capped by device limit)
        float minTessDistance = 20.0f;   // full tess within this distance
        float maxTessDistance = 6000.0f; // tess → 1 at this distance

        //  Animation
        float timeScale = 1.0f;
        glm::vec3 windDirection = glm::vec3(1.0f, 0.0f, 0.3f);

        //  Planet / sun (planetCenter is the planet's center in world/render
        //  coordinates. With the convention "camera starts at sea level at
        //  the origin", planetCenter = (0, -planetRadius, 0) by default.)
        glm::vec3 planetCenter = glm::vec3(0.0f, -6'371'000.0f, 0.0f);
        float planetRadius = 6'371'000.0f;                          // match AtmosphereParams::bottomRadius by default
        glm::vec3 sunDirection = glm::vec3(0.577f, 0.577f, 0.577f); // toward sun, unit vector

        //  Mesh
        // patchCount: coarse grid divisions per axis; total patches = patchCount².
        // 32 → 1024 quad patches; each subdivided by the tessellator at runtime.
        uint32_t patchCount = 32;
        glm::vec2 patchExtent = glm::vec2(16000.0f, 16000.0f);
    };

    //  GPU uniform buffer  –  must match OceanCommon.si layout exactly (std140)
    struct OceanTessellationFrameUBO // offset  size
    {
        // View
        glm::mat4 viewProj;   //   0      64
        glm::mat4 invView;    //  64      64
        glm::mat4 invProj;    // 128      64
        glm::vec3 cameraPos;  // 192      12
        float time;           // 204       4
        glm::vec2 screenSize; // 208       8
        float _pad0;          // 216       4
        float _pad1;          // 220       4
        //  224, 16-byte aligned
        // Wave params  (8 floats = 32 bytes)
        float waveAmplitude;  // 224
        float waveFrequency;  // 228
        float waveSpeed;      // 232
        float waveSteepness;  // 236
        float waveAmplitude2; // 240
        float waveFrequency2; // 244
        float waveSpeed2;     // 248
        float tessFactor;     // 252
        // Tessellation / FFT tiling (4 floats = 16 bytes)
        float minTessDistance; // 256
        float maxTessDistance; // 260
        float tile0;           // 264  (was _pad2) -- 1/lengthScale0
        float tile1;           // 268  (was _pad3) -- 1/lengthScale1
        //  272, 16-byte aligned
        glm::vec3 windDirection; // 272      12
        float timeScale;         // 284       4
        //  288, 16-byte aligned
        // Visual  (each vec3+float = 16 bytes)
        glm::vec3 oceanColor;   // 288      12
        float glossiness;       // 300       4
        glm::vec3 shallowColor; // 304      12
        float specularPower;    // 316       4
        glm::vec3 foamColor;    // 320      12
        float foamIntensity;    // 332       4
        float foamThreshold;    // 336       4
        float tile2;            // 340  (was _pad4) -- 1/lengthScale2
        float tile3;            // 344  (was _pad5) -- 1/lengthScale3
        float normalStrength;   // 348  (was _pad6)
        //  352, 16-byte aligned
        // Planet / sun (vec3+float = 16 bytes, vec3+float = 16 bytes)
        glm::vec3 planetCenter; // 352      12  (always vec3(0) in render coords)
        float planetRadius;     // 364       4
        glm::vec3 sunDirection; // 368      12
        float _pad7;            // 380       4
        //  Total: 384 bytes  (384 / 16 = 24, correctly aligned)
    };
    static_assert(sizeof(OceanTessellationFrameUBO) == 384,
                  "OceanTessellationFrameUBO size mismatch. Check std140 padding");

    class OceanTessellationPipelinePass : public PipelinePass
    {
        /*
        inline static bool s_registered = []()
        {
            PipelinePassInitRegistry::Get().Register(
                [](PipelinePassManager &mgr)
                {
                    mgr.Add<OceanTessellationPipelinePass>(
                        Pipeline::Stage{0, 0}, // was {0, 1}
                        std::make_unique<OceanTessellationPipelinePass>(
                            Pipeline::Stage{0, 0})); // was {0, 1}
                });
            return true;
        }();*/

    public:
        explicit OceanTessellationPipelinePass(Pipeline::Stage stage,
                                               const OceanTessellationParams &params = {});
        ~OceanTessellationPipelinePass() override = default;

        void Render(const CommandBuffer &commandBuffer) override;

        void UpdateFrameData();

        void SetParams(const OceanTessellationParams &params);
        OceanTessellationParams &GetParams() { return params_; }
        const OceanTessellationParams &GetParams() const { return params_; }

        // Rebuild mesh topology when patchCount / patchExtent changes.
        void RebuildMesh();

        void RunFFTPass(const CommandBuffer &cmd);

    private:
        void syncParamsToFrameData(); // copy all params_ → frameData_
        void updateUBO();             // upload frameData_ + spectrum UBO to GPU
        void setupDescriptorSet();
        void setupComputeDescriptorSets();

        OceanTessellationParams params_;
        OceanTessellationFrameUBO frameData_{};
        OceanFFTSettings fftSettings_{};
        float accumulatedTime_ = 0.0f;

        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;
        std::unique_ptr<UniformBuffer> ubo_;
        std::unique_ptr<UniformBuffer> spectrumUBO_;

        // One ComputePipeline + DescriptorSet per kernel, each shader file
        // has a single entry point (see ComputePipeline ctor: shaderStage is
        // a single file, no BindKernel concept). All four point at the same
        // OceanFFT.comp.shader source but select their kernel via a #pragma
        // entry override / define passed at construction.
        std::unique_ptr<ComputePipeline> initSpectrumPipeline_;   // CS_InitializeSpectrum
        std::unique_ptr<ComputePipeline> packConjugatePipeline_;  // CS_PackSpectrumConjugate
        std::unique_ptr<ComputePipeline> updateSpectrumPipeline_; // CS_UpdateSpectrumForFFT
        std::unique_ptr<ComputePipeline> horizontalFFTPipeline_;  // CS_HorizontalFFT
        std::unique_ptr<ComputePipeline> verticalFFTPipeline_;    // CS_VerticalFFT
        std::unique_ptr<ComputePipeline> assemblePipeline_;       // CS_AssembleMaps

        std::unique_ptr<DescriptorSet> initSpectrumDescSet_;
        std::unique_ptr<DescriptorSet> packConjugateDescSet_;
        std::unique_ptr<DescriptorSet> updateSpectrumDescSet_;
        std::unique_ptr<DescriptorSet> horizontalFFTDescSet_;
        std::unique_ptr<DescriptorSet> verticalFFTDescSet_;
        std::unique_ptr<DescriptorSet> assembleDescSet_;

        std::unique_ptr<Image2dArray> initialSpectrumTex_; // 1024x1024x4, rgba32f
        std::unique_ptr<Image2dArray> spectrumTex_;        // 1024x1024x8, rgba32f
        std::unique_ptr<Image2dArray> fourierTarget_;      // 1024x1024x8, rgba32f (FFT scratch)
        std::unique_ptr<Image2dArray> displacementTex_;    // 1024x1024x4, rgba32f
        std::unique_ptr<Image2dArray> slopeTex_;           // 1024x1024x4, rg32f
        std::unique_ptr<Buffer> spectrumParamsBuf_;        // 8 x SpectrumParameters, std430

        std::unique_ptr<OceanClipmapMesh> clipmapMesh_; // replaces mesh_
    };

} // namespace SF::Engine