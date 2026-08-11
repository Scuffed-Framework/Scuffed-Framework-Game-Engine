#pragma once

#include <Rendering/Shaders/Parser/Parser.hpp>
#include <Rendering/Shaders/Shader.hpp>
#include <Rendering/Stage.hpp>
#include <Math/Vectors/Vector.hpp>
#include <array>
#include "Pipeline.hpp"

namespace SF::Engine
{
    class ImageDepth;
    class Image2d;

    /**
     * @brief Class that represents a RenderSystem pipeline.
     */
    class RenderPipeline : public Pipeline
    {
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
         */
        RenderPipeline(Stage stage, std::filesystem::path shaderPath,
                       std::vector<Shader::VertexInput> vertexInputs,
                       std::vector<Shader::Define> defines = {}, Mode mode = Mode::Polygon,
                       Depth depth = Depth::ReadWrite,
                       VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                       VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
                       VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT,
                       VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                       bool pushDescriptors = false);

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
                       VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE);

        ~RenderPipeline();

        /**
         * Gets the depth stencil used in a stage.
         * @param stage The stage to get values from, if not provided the pipelines stage will be
         * used.
         * @return The depth stencil that is found.
         */
        const ImageDepth *GetDepthStencil(
            const std::optional<uint32_t> &stage = std::nullopt) const;

        /**
         * Gets a image used in a stage by the index given to it in the renderpass.
         * @param index The renderpass Image index.
         * @param stage The stage to get values from, if not provided the pipelines stage will be
         * used.
         * @return The image that is found.
         */
        const Image2d *GetImage(uint32_t index,
                                const std::optional<uint32_t> &stage = std::nullopt) const;

        /**
         * Gets the render stage viewport.
         * @param stage The stage to get values from, if not provided the pipelines stage will be
         * used.
         * @return The the render stage viewport.
         */
        RenderArea GetRenderArea(const std::optional<uint32_t> &stage = std::nullopt) const;

        const Stage &GetStage() const
        {
            return stage;
        }
        const std::filesystem::path &GetShaderPath() const
        {
            return shaderPath;
        }
        const std::vector<Shader::VertexInput> &GetVertexInputs() const
        {
            return vertexInputs;
        }
        const std::vector<Shader::Define> &GetDefines() const
        {
            return defines;
        }
        Mode GetMode() const
        {
            return mode;
        }
        Depth GetDepth() const
        {
            return depth;
        }
        VkPrimitiveTopology GetTopology() const
        {
            return topology;
        }
        VkPolygonMode GetPolygonMode() const
        {
            return polygonMode;
        }
        VkCullModeFlags GetCullMode() const
        {
            return cullMode;
        }
        VkFrontFace GetFrontFace() const
        {
            return frontFace;
        }
        bool IsPushDescriptors() const override
        {
            return pushDescriptors;
        }
        const Shader *GetShader() const override
        {
            return shader.get();
        }
        const VkDescriptorSetLayout &GetDescriptorSetLayout() const override
        {
            return descriptorSetLayout;
        }
        const VkDescriptorPool &GetDescriptorPool() const override
        {
            return descriptorPool;
        }
        const VkPipeline &GetPipeline() const override
        {
            return pipeline;
        }
        const VkPipelineLayout &GetPipelineLayout() const override
        {
            return pipelineLayout;
        }
        const VkPipelineBindPoint &GetPipelineBindPoint() const override
        {
            return pipelineBindPoint;
        }

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
        std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachmentStates;
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
                             bool pushDescriptors = false)
            : shaderPath(std::move(shaderPath)),
              vertexInputs(std::move(vertexInputs)),
              defines(std::move(defines)),
              mode(mode),
              depth(depth),
              topology(topology),
              polygonMode(polygonMode),
              cullMode(cullMode),
              frontFace(frontFace),
              pushDescriptors(pushDescriptors)
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
                                      topology, polygonMode, cullMode, frontFace, pushDescriptors);
        }

        const std::filesystem::path &GetShaderPath() const
        {
            return shaderPath;
        }
        const std::vector<Shader::VertexInput> &GetVertexInputs() const
        {
            return vertexInputs;
        }
        const std::vector<Shader::Define> &GetDefines() const
        {
            return defines;
        }
        RenderPipeline::Mode GetMode() const
        {
            return mode;
        }
        RenderPipeline::Depth GetDepth() const
        {
            return depth;
        }
        VkPrimitiveTopology GetTopology() const
        {
            return topology;
        }
        VkPolygonMode GetPolygonMode() const
        {
            return polygonMode;
        }
        VkCullModeFlags GetCullMode() const
        {
            return cullMode;
        }
        VkFrontFace GetFrontFace() const
        {
            return frontFace;
        }
        bool GetPushDescriptors() const
        {
            return pushDescriptors;
        }

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
    };
}