#pragma once

#include <Rendering/Shaders/Parser/Parser.hpp>
#include <Rendering/Shaders/Shader.hpp>
#include <Rendering/Stage.hpp>
#include <Math/Vectors/Vector.hpp>
#include <array>
#include "Pipeline.hpp"

// some linux include defines None
#ifdef None
#undef None
#endif

namespace SF::Engine
{
    class ImageDepth;
    class Image2d;

    /**
     * @brief Class that represents a RenderSystem pipeline.
     */
    class RenderPipeline : public Pipeline {
    public:
        enum class Mode
        {
            Polygon,
            MRT
        };

        enum class Depth
        {
            None = 0,
            Read = 1,
            Write = 2,
            ReadWrite = Read | Write
        };

        /**
         * @brief Common blend presets, mirroring the built-in surface blend modes
         * you'd find in Unity (Opaque / Alpha / Premultiply / Additive / Multiply).
         * Use Custom + the `blendStates` constructor parameter for anything these
         * presets don't cover, or when different attachments in an MRT pipeline
         * need different blend equations.
         */
        enum class Blend
        {
            Opaque,             ///< blendEnable = false, straight overwrite.
            AlphaBlend,         ///< "Straight"/non-premultiplied alpha: src*srcA + dst*(1-srcA)
            PremultipliedAlpha, ///< src*1 + dst*(1-srcA). Use when the shader outputs premultiplied colour.
            Additive,           ///< src*srcA + dst*1
            Multiply,           ///< src*dst
            Screen,             ///< src + dst - src*dst
            Custom              ///< Ignore this enum; use the explicit `blendStates` vector as-is.
        };

        /**
         * Builds a single VkPipelineColorBlendAttachmentState for one of the Blend presets.
         * Public so callers building a Custom vector can start from a preset and tweak it.
         */
        [[nodiscard]] static VkPipelineColorBlendAttachmentState MakeBlendAttachmentState(
            Blend preset,
            VkColorComponentFlags writeMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

        /**
         * Creates a new pipeline.
         * @param stage The RenderSystem stage this pipeline will be run on.
         * @param shaderPath Path to the .shader file to load.
         * @param vertexInputs The vertex inputs that will be used as a shaders input.
         * @param defines A list of defines added to the top of each shader.
         * @param mode The mode this pipeline will run in.
         * @param depth The depth read/write that will be used.
         * @param topology The topology of the input assembly.
         * @param polygonMode The polygon draw mode.
         * @param cullMode The vertex cull mode.
         * @param frontFace The direction to render faces.
         * @param pushDescriptors If no actual descriptor sets are allocated but instead pushed.
         * @param additionalLayouts Extra descriptor set layouts appended after set 0 (this
         *        pipeline's own set) and set 1 (shared samplers); e.g. a per-material or
         *        per-pass set.
         * @param blend Blend preset applied to every colour attachment. In MRT mode every
         *        attachment shares this preset unless you pass Blend::Custom.
         * @param blendStates Only consulted when `blend == Blend::Custom`. Must contain exactly
         *        1 entry for Mode::Polygon, or exactly `attachmentCount` entries for Mode::MRT.
         */
        RenderPipeline(Stage stage, std::filesystem::path shaderPath,
                       std::vector<Shader::VertexInput> vertexInputs,
                       std::vector<Shader::Define> defines = {}, Mode mode = Mode::Polygon,
                       Depth depth = Depth::ReadWrite,
                       VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                       VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
                       VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
                       VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                       bool pushDescriptors = false,
                       std::vector<VkDescriptorSetLayout> additionalLayouts = {},
                       Blend blend = Blend::PremultipliedAlpha,
                       std::vector<VkPipelineColorBlendAttachmentState> blendStates = {}
                       );

        /**
         * Offscreen constructor : uses a caller-supplied VkRenderPass instead of
         * querying one from the RenderSystem stage.  Use this for one-shot LUT
         * bake pipelines that render into images unrelated to the swap chain.
         *
         * IsMultisampled() always returns false for offscreen pipelines.
         */
        RenderPipeline(VkRenderPass offscreenRenderPass, uint32_t subpassIndex,
                       std::filesystem::path shaderPath,
                       std::vector<Shader::VertexInput> vertexInputs = {},
                       std::vector<Shader::Define> defines = {},
                       Depth depth = Depth::None,
                       VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                       VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
                       VkCullModeFlags cullMode = VK_CULL_MODE_NONE,
                       VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                       std::vector<VkDescriptorSetLayout> additionalLayouts = {},
                       Blend blend = Blend::Opaque,
                       std::vector<VkPipelineColorBlendAttachmentState> blendStates = {}
                       );

        ~RenderPipeline() override;

        /**
         * Gets the depth stencil used in a stage. Offscreen pipelines must pass an
         * explicit `stage`; they aren't tied to a RenderSystem stage.
         */
        [[nodiscard]] const ImageDepth *GetDepthStencil(
            const std::optional<uint32_t> &stage = std::nullopt) const;

        [[nodiscard]] const Image2d *GetImage(uint32_t index,
                                const std::optional<uint32_t> &stage = std::nullopt) const;

        [[nodiscard]] RenderArea GetRenderArea(const std::optional<uint32_t> &stage = std::nullopt) const;

        [[nodiscard]] const Stage &GetStage() const { return stage; }
        [[nodiscard]] const std::filesystem::path &GetShaderPath() const { return shaderPath; }
        [[nodiscard]] const std::vector<Shader::VertexInput> &GetVertexInputs() const { return vertexInputs; }
        [[nodiscard]] const std::vector<Shader::Define> &GetDefines() const { return defines; }
        [[nodiscard]] Mode GetMode() const { return mode; }
        [[nodiscard]] Depth GetDepth() const { return depth; }
        [[nodiscard]] Blend GetBlend() const { return blend; }
        [[nodiscard]] VkPrimitiveTopology GetTopology() const { return topology; }
        [[nodiscard]] VkPolygonMode GetPolygonMode() const { return polygonMode; }
        [[nodiscard]] VkCullModeFlags GetCullMode() const { return cullMode; }
        [[nodiscard]] VkFrontFace GetFrontFace() const { return frontFace; }
        [[nodiscard]] bool IsOffscreen() const { return isOffscreen_; }
        [[nodiscard]] bool IsPushDescriptors() const override { return pushDescriptors; }
        [[nodiscard]] const Shader *GetShader() const override { return shader.get(); }
        [[nodiscard]] const VkDescriptorSetLayout &GetDescriptorSetLayout() const override { return descriptorSetLayout; }
        [[nodiscard]] const VkDescriptorPool &GetDescriptorPool() const override { return descriptorPool; }
        [[nodiscard]] const VkPipeline &GetPipeline() const override { return pipeline; }
        [[nodiscard]] const VkPipelineLayout &GetPipelineLayout() const override { return pipelineLayout; }
        [[nodiscard]] const VkPipelineBindPoint &GetPipelineBindPoint() const override { return pipelineBindPoint; }

    private:
        void CreateShaderProgram();
        void CreateDescriptorLayout();
        void CreateDescriptorLayout_UpdateAfterBind();
        void CreateDescriptorPool();
        void CreatePipelineLayout();
        void CreateAttributes();
        void CreatePipeline();
        void CreatePipelinePolygon();
        void CreatePipelineMrt();

        Stage stage;
        std::filesystem::path shaderPath;
        std::vector<Shader::VertexInput> vertexInputs;
        std::vector<Shader::Define> defines;
        Mode mode;
        Depth depth;
        VkPrimitiveTopology topology;
        VkPolygonMode polygonMode;
        VkCullModeFlags cullMode;
        VkFrontFace frontFace;
        bool pushDescriptors;

        std::shared_ptr<Shader> shader;
        std::vector<VkDynamicState> dynamicStates;
        std::vector<VkPipelineShaderStageCreateInfo> stages;

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint;

        // Raw device handle stored at construction so the destructor never needs
        // to call RenderSystem::Get() on a potentially-destroyed singleton.
        VkDevice device_ = VK_NULL_HANDLE;

        // Non-null only for the offscreen constructor : used directly in CreatePipeline()
        VkRenderPass offscreenRenderPass_ = VK_NULL_HANDLE;
        uint32_t offscreenSubpass_ = 0;
        bool isOffscreen_ = false;

        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
        VkPipelineRasterizationStateCreateInfo rasterizationState = {};

        Blend blend = Blend::PremultipliedAlpha;
        std::vector<VkPipelineColorBlendAttachmentState> customBlendStates = {}; // only used when blend == Custom
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates = {}; // resolved states actually used
        std::vector<VkDescriptorSetLayout> additionalLayouts {};

        VkPipelineColorBlendStateCreateInfo colourBlendState = {};
        VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
        VkPipelineViewportStateCreateInfo viewportState = {};
        VkPipelineMultisampleStateCreateInfo multisampleState = {};
        VkPipelineDynamicStateCreateInfo dynamicState = {};
        VkPipelineTessellationStateCreateInfo tessellationState = {};
    };

    class RenderPipelineCreate
    {
    public:
        RenderPipelineCreate(std::filesystem::path shaderPath = {},
                             std::vector<Shader::VertexInput> vertexInputs = {},
                             std::vector<Shader::Define> defines = {},
                             RenderPipeline::Mode mode = RenderPipeline::Mode::Polygon,
                             RenderPipeline::Depth depth = RenderPipeline::Depth::ReadWrite,
                             VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                             VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
                             VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
                             VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                             bool pushDescriptors = false,
                             std::vector<VkDescriptorSetLayout> additionalLayouts = {},
                             RenderPipeline::Blend blend = RenderPipeline::Blend::PremultipliedAlpha,
                             std::vector<VkPipelineColorBlendAttachmentState> blendStates = {})
            : shaderPath(std::move(shaderPath)),
              vertexInputs(std::move(vertexInputs)),
              defines(std::move(defines)),
              mode(mode),
              depth(depth),
              topology(topology),
              polygonMode(polygonMode),
              cullMode(cullMode),
              frontFace(frontFace),
              pushDescriptors(pushDescriptors),
              additionalLayouts(std::move(additionalLayouts)),
              blend(blend),
              blendStates(std::move(blendStates))
        {
        }

        /**
         * Creates a new pipeline.
         * @param pipelineStage The pipelines RenderSystem stage.
         * @return The created RenderSystem pipeline.
         */
        RenderPipeline *Create(const Pipeline::Stage &pipelineStage) const
        {
            return new RenderPipeline(pipelineStage, shaderPath, vertexInputs, defines, mode, depth,
                                      topology, polygonMode, cullMode, frontFace, pushDescriptors,
                                      additionalLayouts, blend, blendStates);
        }

        [[nodiscard]] const std::filesystem::path &GetShaderPath() const { return shaderPath; }
        [[nodiscard]] const std::vector<Shader::VertexInput> &GetVertexInputs() const { return vertexInputs; }
        [[nodiscard]] const std::vector<Shader::Define> &GetDefines() const { return defines; }
        [[nodiscard]] RenderPipeline::Mode GetMode() const { return mode; }
        [[nodiscard]] RenderPipeline::Depth GetDepth() const { return depth; }
        [[nodiscard]] VkPrimitiveTopology GetTopology() const { return topology; }
        [[nodiscard]] VkPolygonMode GetPolygonMode() const { return polygonMode; }
        [[nodiscard]] VkCullModeFlags GetCullMode() const { return cullMode; }
        [[nodiscard]] VkFrontFace GetFrontFace() const { return frontFace; }
        [[nodiscard]] bool GetPushDescriptors() const { return pushDescriptors; }
        [[nodiscard]] const std::vector<VkDescriptorSetLayout> &GetAdditionalLayouts() const { return additionalLayouts; }
        [[nodiscard]] RenderPipeline::Blend GetBlend() const { return blend; }
        [[nodiscard]] const std::vector<VkPipelineColorBlendAttachmentState> &GetBlendStates() const { return blendStates; }

    private:
        std::filesystem::path shaderPath;
        std::vector<Shader::VertexInput> vertexInputs;
        std::vector<Shader::Define> defines;

        RenderPipeline::Mode mode;
        RenderPipeline::Depth depth;
        VkPrimitiveTopology topology;
        VkPolygonMode polygonMode;
        VkCullModeFlags cullMode;
        VkFrontFace frontFace;
        bool pushDescriptors;
        std::vector<VkDescriptorSetLayout> additionalLayouts;
        RenderPipeline::Blend blend;
        std::vector<VkPipelineColorBlendAttachmentState> blendStates;
    };
}