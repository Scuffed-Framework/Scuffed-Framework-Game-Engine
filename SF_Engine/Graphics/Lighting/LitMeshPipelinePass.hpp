#pragma once

#include <Graphics/PipelinePassManager.hpp>
#include <Graphics/Pipelines/RenderPipeline.hpp>
#include <Graphics/Descriptors/DescriptorSet.hpp>
#include <Graphics/Images/Image2d.hpp>
#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Mesh/MeshFactory.hpp>
#include "LightManager.hpp"
#include "LightingTypes.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <XML/XMLReader.hpp>

namespace SF::Engine
{
    //  Push constant layout matching Lit.shader
    // Fits in 128 bytes (Vulkan minimum guarantee):
    //   model mat4     = 64 bytes
    //   baseColor vec4 = 16 bytes
    //   4 floats       = 16 bytes
    //   Total          = 96 bytes
    // The normal matrix is derived in the vertex shader via inverse-transpose
    // of the model matrix, avoiding a second mat4 in push constants.
    struct alignas(4) LitPushConstants
    {
        glm::mat4 model;
        glm::vec4 baseColor = {1, 1, 1, 1};
        float roughnessFactor = 1.0f;
        float metallicFactor = 0.0f;
        float aoFactor = 1.0f;
        float emissiveFactor = 0.0f;
    };
    static_assert(sizeof(LitPushConstants) == 96,
                  "LitPushConstants must be 96 bytes");
    static_assert(sizeof(LitPushConstants) <= 128,
                  "LitPushConstants exceeds minimum guaranteed push constant size");

    //  Per-mesh material textures + constants
    struct MeshMaterial : public Serializable
    {
        std::shared_ptr<Image2d> albedo;   // bind=4  (white if null)
        std::shared_ptr<Image2d> normal;   // bind=5  (flat if null)
        std::shared_ptr<Image2d> pbr;      // bind=6  (r=rough g=metal b=ao, white if null)
        std::shared_ptr<Image2d> emissive; // bind=7  (black if null)

        glm::vec4 baseColor = {1, 1, 1, 1};
        float roughnessFactor = 1.0f;
        float metallicFactor = 0.0f;
        float aoFactor = 1.0f;
        float emissiveFactor = 0.0f;

        void Serialize(XMLNode &node) const override
        {
            // Only serialize if the texture exists
            if (albedo)
            {
                XMLNode n = node.AddChild("albedo");
                albedo->Serialize(n);
            }
            if (normal)
            {
                XMLNode n = node.AddChild("normal");
                normal->Serialize(n);
            }
            if (pbr)
            {
                XMLNode n = node.AddChild("pbr");
                pbr->Serialize(n);
            }
            if (emissive)
            {
                XMLNode n = node.AddChild("emissive");
                emissive->Serialize(n);
            }

            node.SetAttribute("roughnessFactor", roughnessFactor);
            node.SetAttribute("metallicFactor", metallicFactor);
            node.SetAttribute("aoFactor", aoFactor);
            node.SetAttribute("emissiveFactor", emissiveFactor);

            XMLNode colorNode = node.AddChild("baseColor");
            colorNode.SetAttribute("r", baseColor.r);
            colorNode.SetAttribute("g", baseColor.g);
            colorNode.SetAttribute("b", baseColor.b);
            colorNode.SetAttribute("a", baseColor.a);
        }

        void Deserialize(const XMLNode &node) override
        {
            // Only deserialize if the child node exists
            if (XMLNode n = node.GetChild("albedo"); n.IsValid())
            {
                albedo = std::make_shared<Image2d>("", VK_FILTER_LINEAR,
                                                   VK_SAMPLER_ADDRESS_MODE_REPEAT, true, true, false);
                albedo->Deserialize(n);
            }
            if (XMLNode n = node.GetChild("normal"); n.IsValid())
            {
                normal = std::make_shared<Image2d>("", VK_FILTER_LINEAR,
                                                   VK_SAMPLER_ADDRESS_MODE_REPEAT, true, true, false);
                normal->Deserialize(n);
            }
            if (XMLNode n = node.GetChild("pbr"); n.IsValid())
            {
                pbr = std::make_shared<Image2d>("", VK_FILTER_LINEAR,
                                                VK_SAMPLER_ADDRESS_MODE_REPEAT, true, true, false);
                pbr->Deserialize(n);
            }
            if (XMLNode n = node.GetChild("emissive"); n.IsValid())
            {
                emissive = std::make_shared<Image2d>("", VK_FILTER_LINEAR,
                                                     VK_SAMPLER_ADDRESS_MODE_REPEAT, true, true, false);
                emissive->Deserialize(n);
            }

            node.GetAttribute("roughnessFactor", roughnessFactor);
            node.GetAttribute("metallicFactor", metallicFactor);
            node.GetAttribute("aoFactor", aoFactor);
            node.GetAttribute("emissiveFactor", emissiveFactor);

            XMLNode colorNode = node.GetChild("baseColor");
            if (colorNode.IsValid())
            {
                colorNode.GetAttribute("r", baseColor.r);
                colorNode.GetAttribute("g", baseColor.g);
                colorNode.GetAttribute("b", baseColor.b);
                colorNode.GetAttribute("a", baseColor.a);
            }
        }
    };

    //  Drawable instance
    struct MeshInstance
    {
        std::shared_ptr<Mesh> mesh;
        MeshMaterial material;
        glm::mat4 transform = glm::mat4(1.0f);
    };

    /**
     * @brief Forward-lit opaque mesh PipelinePass using Lit.shader.
     *
     * This is the single-pass equivalent of "Lit" in Unity or the default
     * material in Unreal. It evaluates the full clustered light list per-fragment
     * and applies Cook-Torrance PBR + ACES tonemap.
     *
     * Usage:
     *   auto* litPass = AddPipelinePass<LitMeshPipelinePass>({stageIdx, 0}, *lightManager);
     *   litPass->Submit(myMesh, myMaterial, modelMatrix);
     *   // Each frame the Submit list is drawn then cleared.
     *
     * Descriptor layout (flat set=0, matches Lit.shader):
     *   bind 0  UBO   GpuFrameData
     *   bind 1  SSBO  GpuLight[]
     *   bind 2  SSBO  GpuClusterLightList[]
     *   bind 3  SSBO  uint lightIndices[]
     *   bind 4  sampler2D albedoMap    ┐
     *   bind 5  sampler2D normalMap    │ per-mesh, written fresh each draw
     *   bind 6  sampler2D pbrMap       │
     *   bind 7  sampler2D emissiveMap  ┘
     */
    class LitMeshPipelinePass : public PipelinePass
    {
    public:
        explicit LitMeshPipelinePass(Pipeline::Stage stage, LightManager &lightManager);
        ~LitMeshPipelinePass() override = default;

        /// Queue a mesh+material+transform for drawing this frame.
        void Submit(std::shared_ptr<Mesh> mesh,
                    const MeshMaterial &material,
                    const glm::mat4 &transform);

        void Submit(const MeshInstance &instance);

        void Render(const CommandBuffer &commandBuffer) override;

    private:
        void WriteFrameDescriptors();
        void WriteMaterialDescriptors(const MeshMaterial &material);
        Image2d *GetFallback(int slot); // returns 1x1 white or flat-normal texture

        LightManager &lm_;

        std::unique_ptr<RenderPipeline> pipeline_;
        std::unique_ptr<DescriptorSet> descSet_;

        // Fallback 1x1 textures so shaders never get unbound samplers
        std::unique_ptr<Image2d> fallbackWhite_;
        std::unique_ptr<Image2d> fallbackNormal_;

        struct DrawCall
        {
            std::shared_ptr<Mesh> mesh;
            MeshMaterial material;
            glm::mat4 transform;
        };
        std::vector<DrawCall> drawList_;

        bool frameDescriptorsWritten_ = false;
    };
}
