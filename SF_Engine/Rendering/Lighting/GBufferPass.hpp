#pragma once

#include <Rendering/FrameGraph/EngineRenderpassManager.hpp>
#include <Math/BasicMath.hpp>
#include <Rendering/Mesh/Mesh.hpp>
#include <Rendering/RHI/Descriptors/DescriptorSet.hpp>
#include <Rendering/RHI/Images/Image2d.hpp>
#include <Rendering/RHI/Pipelines/RhiRenderPipeline.hpp>
#include <memory>
#include <vector>
#include "LightManager.hpp"
#include "LitMeshPipelinePass.hpp" // reuses MeshMaterial and LitPushConstants

namespace SF::Engine
{
    /**
     * @brief Geometry pass : writes the GBuffer (deferred pipeline).
     *
     * Use this with LightingRenderer's deferred path.
     * For simple forward rendering, use LitMeshPipelinePass instead.
     *
     * Render stage expected (stage 0):
     *   attach 0  "gbuf_depth"  Depth
     *   attach 1  "gbuf_albedo" Image VK_FORMAT_R8G8B8A8_UNORM
     *   attach 2  "gbuf_normal" Image VK_FORMAT_R16G16_SNORM
     *   attach 3  "gbuf_pbr"    Image VK_FORMAT_R8G8B8A8_UNORM
     *   SubpassType{0, {0,1,2,3}}
     *
     * Flat descriptor layout (set=0, matches GBuffer.shader):
     *   bind 0  UBO   GpuFrameData
     *   bind 1  sampler2D albedoMap
     *   bind 2  sampler2D normalMap
     *   bind 3  sampler2D pbrMap
     *   bind 4  sampler2D emissiveMap
     *
     * Push constants: LitPushConstants (model, normalMatrix, material scalars)
     */
    class GBufferPass : public EngineRenderpass
    {
    public:
        explicit GBufferPass(Pipeline::Stage stage, LightManager &lightManager);
        ~GBufferPass() override = default;

        /// Queue a mesh for rendering this frame.
        void Submit(std::shared_ptr<Mesh> mesh, const MeshMaterial &material, const Mat4 &transform);

        void Render(const CommandBuffer &commandBuffer) override;

        RhiRenderPipeline &GetPipeline() { return *pipeline_; }

    private:
        void WriteFrameDescriptors();
        void WriteMaterialDescriptors(const MeshMaterial &mat);

        LightManager &lm_;

        std::unique_ptr<RhiRenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;

        std::unique_ptr<Image2d> fallbackWhite_;
        std::unique_ptr<Image2d> fallbackNormal_;

        struct DrawCall
        {
            std::shared_ptr<Mesh> mesh;
            MeshMaterial material;
            Mat4 transform;
        };
        std::vector<DrawCall> drawList_;
    };
} // namespace SF::Engine
