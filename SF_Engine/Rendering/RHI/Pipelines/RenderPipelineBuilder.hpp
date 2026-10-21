#pragma once

#include <UtilityClasses/Patterns.hpp>
#include <memory>
#include <vector>
#include "RhiRenderPipeline.hpp"

namespace SF::Engine
{
    /**
     * @brief Fluent construction settings for a RhiRenderPipeline, mirroring
     * DescriptorSetWriteBuilder's style (RHI/Descriptors/DescriptorSetBuilder.hpp) the same way
     * ComputePipelineBuilder does.
     *
     * RhiRenderPipeline (like ComputePipeline) does its real work in its constructor, so this
     * builder's finishing step is Create()/CreateOffscreen() rather than the base
     * BuilderPattern::Build(), which is still available if you just want the raw
     * RenderPipelineDesc (e.g. to feed RenderPipelineCreate::Create() elsewhere, or to stash
     * and diff against a later config).
     *
     * Usage (normal, tied to a RenderSystem stage/subpass):
     *   auto pipeline = RenderPipelineBuilder()
     *       .Shader("Shaders/Atmosphere/AtmosphereComposite.shader")
     *       .Depth(RhiRenderPipeline::Depth::None)
     *       .CullMode(VK_CULL_MODE_NONE)
     *       .Create(stage);
     *
     * Usage (offscreen, e.g. a one-shot LUT bake):
     *   auto pipeline = RenderPipelineBuilder()
     *       .Shader("Shaders/Atmosphere/LUT/SkyView.shader")
     *       .Create(RenderPipelineBuilder::Offscreen{lutRenderPass, 0});
     */
    struct RenderPipelineDesc
    {
        std::filesystem::path shaderPath;
        std::vector<Shader::VertexInput> vertexInputs;
        std::vector<Shader::Define> defines;
        RhiRenderPipeline::Mode mode   = RhiRenderPipeline::Mode::Polygon;
        RhiRenderPipeline::Depth depth = RhiRenderPipeline::Depth::ReadWrite;
        VkPrimitiveTopology topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPolygonMode polygonMode      = VK_POLYGON_MODE_FILL;
        VkCullModeFlags cullMode       = VK_CULL_MODE_BACK_BIT;
        VkFrontFace frontFace          = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        bool pushDescriptors           = false;
        std::vector<VkDescriptorSetLayout> additionalLayouts;
        RhiRenderPipeline::Blend blend = RhiRenderPipeline::Blend::PremultipliedAlpha;
        std::vector<VkPipelineColorBlendAttachmentState> blendStates; // only consulted for Blend::Custom
    };

    class RenderPipelineBuilder : public BuilderPattern<RenderPipelineBuilder, RenderPipelineDesc>
    {
    public:
        // Tags an offscreen render pass + subpass for the CreateOffscreen()-equivalent overload
        // of Create(), so the same fluent chain can finish either way without two differently
        // named terminal methods reading awkwardly at the call site.
        struct Offscreen
        {
            VkRenderPass renderPass;
            uint32_t subpassIndex = 0;
        };

        RenderPipelineBuilder &Shader(std::filesystem::path path)
        {
            m_Product.shaderPath = std::move(path);
            return Self();
        }

        RenderPipelineBuilder &VertexInput(Shader::VertexInput input)
        {
            m_Product.vertexInputs.push_back(std::move(input));
            return Self();
        }

        RenderPipelineBuilder &VertexInputs(std::vector<Shader::VertexInput> inputs)
        {
            m_Product.vertexInputs = std::move(inputs);
            return Self();
        }

        RenderPipelineBuilder &Define(std::string name, std::string value = "")
        {
            m_Product.defines.emplace_back(std::move(name), std::move(value));
            return Self();
        }

        RenderPipelineBuilder &Defines(std::vector<Shader::Define> defines)
        {
            m_Product.defines = std::move(defines);
            return Self();
        }

        RenderPipelineBuilder &Mode(RhiRenderPipeline::Mode mode)
        {
            m_Product.mode = mode;
            return Self();
        }

        RenderPipelineBuilder &Depth(RhiRenderPipeline::Depth depth)
        {
            m_Product.depth = depth;
            return Self();
        }

        RenderPipelineBuilder &Topology(VkPrimitiveTopology topology)
        {
            m_Product.topology = topology;
            return Self();
        }

        RenderPipelineBuilder &PolygonMode(VkPolygonMode mode)
        {
            m_Product.polygonMode = mode;
            return Self();
        }

        RenderPipelineBuilder &CullMode(VkCullModeFlags mode)
        {
            m_Product.cullMode = mode;
            return Self();
        }

        RenderPipelineBuilder &FrontFace(VkFrontFace face)
        {
            m_Product.frontFace = face;
            return Self();
        }

        RenderPipelineBuilder &PushDescriptors(bool value = true)
        {
            m_Product.pushDescriptors = value;
            return Self();
        }

        RenderPipelineBuilder &AdditionalLayout(VkDescriptorSetLayout layout)
        {
            m_Product.additionalLayouts.push_back(layout);
            return Self();
        }

        RenderPipelineBuilder &AdditionalLayouts(std::vector<VkDescriptorSetLayout> layouts)
        {
            m_Product.additionalLayouts = std::move(layouts);
            return Self();
        }

        RenderPipelineBuilder &Blend(RhiRenderPipeline::Blend blend)
        {
            m_Product.blend = blend;
            return Self();
        }

        // Only consulted when Blend(RhiRenderPipeline::Blend::Custom) is also set; see
        // RhiRenderPipeline's constructor docs for the exact count required (1 for
        // Mode::Polygon, attachmentCount for Mode::MRT).
        RenderPipelineBuilder &BlendStates(std::vector<VkPipelineColorBlendAttachmentState> states)
        {
            m_Product.blendStates = std::move(states);
            return Self();
        }

        // Constructs the real RhiRenderPipeline tied to a RenderSystem stage/subpass.
        [[nodiscard]] std::unique_ptr<RhiRenderPipeline> Create(Pipeline::Stage stage) const
        {
            return std::make_unique<RhiRenderPipeline>(
                    std::move(stage), m_Product.shaderPath, m_Product.vertexInputs, m_Product.defines, m_Product.mode,
                    m_Product.depth, m_Product.topology, m_Product.polygonMode, m_Product.cullMode,
                    m_Product.frontFace, m_Product.pushDescriptors, m_Product.additionalLayouts, m_Product.blend,
                    m_Product.blendStates);
        }

        // Constructs the real RhiRenderPipeline against a caller-supplied offscreen
        // VkRenderPass instead of a RenderSystem stage (e.g. one-shot LUT bakes) -- see
        // RhiRenderPipeline's offscreen constructor. Note: Mode is ignored here; the offscreen
        // constructor is always Mode::Polygon.
        [[nodiscard]] std::unique_ptr<RhiRenderPipeline> Create(Offscreen target) const
        {
            return std::make_unique<RhiRenderPipeline>(target.renderPass, target.subpassIndex, m_Product.shaderPath,
                                                       m_Product.vertexInputs, m_Product.defines, m_Product.depth,
                                                       m_Product.topology, m_Product.polygonMode, m_Product.cullMode,
                                                       m_Product.frontFace, m_Product.additionalLayouts,
                                                       m_Product.blend, m_Product.blendStates);
        }
    };
} // namespace SF::Engine
