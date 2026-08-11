#pragma once

#include <Rendering/PipelinePassManager.hpp>
#include <Rendering/Pipelines/RenderPipeline.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <Rendering/Images/Image2d.hpp>
#include <Rendering/Images/ImageDepth.hpp>
#include <Rendering/Mesh/Mesh.hpp>
#include "LightManager.hpp"
#include <Math/BasicMath.hpp>
#include <memory>
#include <vector>

namespace SF::Engine
{
    // Push constants for ForwardTransparent.shader : 96 bytes
    struct alignas(4) TransparentPushConstants
    {
        Mat4 model;
        Vec4 baseColor = {1, 1, 1, 1};
        float roughness = 0.05f;
        float metallic = 0.0f;
        float ior = 1.5f; // glass default
        float refractionStrength = 0.02f;
    };
    static_assert(sizeof(TransparentPushConstants) == 96);

    /**
     * @brief Forward pass for transparent objects (glass, water, particles).
     *
     * Single descriptor set layout:
     *   bind 0  UBO   GpuFrameData
     *   bind 1  SSBO  GpuLight[]
     *   bind 2  SSBO  GpuClusterLightList[]
     *   bind 3  SSBO  uint lightIndices[]
     *   bind 4  sampler2D sceneHDR    : lit opaque scene (for refraction)
     *   bind 5  sampler2D sceneDepth  : opaque depth (for soft particles)
     *
     * Depth: Read-only (transparent fragments test but don't write depth).
     * Blending: Premultiplied alpha (engine default in RenderPipeline::CreateAttributes).
     * Culling: None (glass is double-sided).
     *
     * Call BeginFrame() once per frame from your scene/mesh system, then issue
     * per-object draw calls with push constants for model matrix + material.
     */
    class ForwardTransparentPipelinePass : public PipelinePass
    {
    public:
        explicit ForwardTransparentPipelinePass(Pipeline::Stage stage, LightManager &lightManager);
        ~ForwardTransparentPipelinePass() override = default;

        void Render(const CommandBuffer &commandBuffer) override;
        void BeginFrame(const CommandBuffer &commandBuffer);

    private:
        void RefreshSceneDescriptors();

        LightManager &lm_;
        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;

        const Image2d *lastHDR_ = nullptr;
        const ImageDepth *lastDepth_ = nullptr;
    };
}
