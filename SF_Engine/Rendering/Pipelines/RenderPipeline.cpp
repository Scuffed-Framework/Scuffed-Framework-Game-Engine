#include "RenderPipeline.hpp"
#include <Engine/Log/Log.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Rendering/Shaders/Parser/Parser.hpp>
#include <Engine/Engine.hpp>
#include <filesystem>

namespace SF::Engine
{
    const std::vector<VkDynamicState> DYNAMIC_STATES = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH};

    RenderPipeline::RenderPipeline(Stage stage, std::filesystem::path shaderPath,
                                   std::vector<Shader::VertexInput> vertexInputs,
                                   std::vector<Shader::Define> defines, Mode mode, Depth depth,
                                   VkPrimitiveTopology topology, VkPolygonMode polygonMode,
                                   VkCullModeFlags cullMode, VkFrontFace frontFace,
                                   bool pushDescriptors)
        : stage(std::move(stage)),
          shaderPath(std::move(shaderPath)),
          vertexInputs(std::move(vertexInputs)),
          defines(std::move(defines)),
          mode(mode),
          depth(depth),
          topology(topology),
          polygonMode(polygonMode),
          cullMode(cullMode),
          frontFace(frontFace),
          pushDescriptors(pushDescriptors),
          dynamicStates(DYNAMIC_STATES),
          pipelineBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
        device_ = *RenderSystem::Get()->GetLogicalDevice();
        std::sort(this->vertexInputs.begin(), this->vertexInputs.end());
        CreateShaderProgram();
        // Use UPDATE_AFTER_BIND so material texture descriptors can be written
        // while the previous frame's command buffer is still pending (GPU pipelining).

        CreateDescriptorLayout_UpdateAfterBind();
        CreateDescriptorPool();
        CreatePipelineLayout();
        CreateAttributes();

        switch (mode)
        {
        case Mode::Polygon:
            CreatePipelinePolygon();
            break;
        case Mode::MRT:
            CreatePipelineMrt();
            break;
        default:
            throw std::runtime_error("Unknown pipeline mode");
        }
    }

    RenderPipeline::RenderPipeline(VkRenderPass offscreenRenderPass, uint32_t subpassIndex,
                                   std::filesystem::path shaderPath,
                                   std::vector<Shader::VertexInput> vertexInputs,
                                   std::vector<Shader::Define> defines,
                                   Depth depth,
                                   VkPrimitiveTopology topology,
                                   VkPolygonMode polygonMode,
                                   VkCullModeFlags cullMode,
                                   VkFrontFace frontFace)
        : stage({0, subpassIndex}),
          shaderPath(std::move(shaderPath)),
          vertexInputs(std::move(vertexInputs)),
          defines(std::move(defines)),
          mode(Mode::Polygon),
          depth(depth),
          topology(topology),
          polygonMode(polygonMode),
          cullMode(cullMode),
          frontFace(frontFace),
          pushDescriptors(false),
          dynamicStates(DYNAMIC_STATES),
          pipelineBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS),
          offscreenRenderPass_(offscreenRenderPass),
          offscreenSubpass_(subpassIndex),
          isOffscreen_(true)
    {
        device_ = *RenderSystem::Get()->GetLogicalDevice();
        std::sort(this->vertexInputs.begin(), this->vertexInputs.end());
        CreateShaderProgram();
        CreateDescriptorLayout_UpdateAfterBind();
        CreateDescriptorPool();
        CreatePipelineLayout();
        CreateAttributes();
        CreatePipelinePolygon();
    }

    RenderPipeline::~RenderPipeline()
    {
        // Use the stored raw device handle : never call RenderSystem::Get() here.
        // vkDestroy* with VK_NULL_HANDLE handles is a safe no-op per spec.
        if (device_ == VK_NULL_HANDLE)
            return;
        vkDestroyDescriptorPool(device_, descriptorPool, nullptr);
        vkDestroyPipeline(device_, pipeline, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout, nullptr);
    }

    const ImageDepth *RenderPipeline::GetDepthStencil(const std::optional<uint32_t> &stage) const
    {
        return RenderSystem::Get()
            ->GetRenderStage(stage ? *stage : this->stage.first)
            ->GetDepthStencil();
    }

    const Image2d *RenderPipeline::GetImage(uint32_t index,
                                            const std::optional<uint32_t> &stage) const
    {
        return RenderSystem::Get()
            ->GetRenderStage(stage ? *stage : this->stage.first)
            ->GetFramebuffer()
            ->GetAttachment(index);
    }

    RenderArea RenderPipeline::GetRenderArea(const std::optional<uint32_t> &stage) const
    {
        return RenderSystem::Get()
            ->GetRenderStage(stage ? *stage : this->stage.first)
            ->GetRenderArea();
    }

    void RenderPipeline::CreateShaderProgram()
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();

        Shaders::ShaderParser parser;
        Log::Info("Loading shader: {} (cwd={})", shaderPath.string(),
                  GetExecutablePath().string());
        auto parsedShaderOpt = parser.parse(std::filesystem::path(GetExecutablePath() / shaderPath).string());

        if (!parsedShaderOpt)
            throw std::runtime_error("Failed to parse shader '" + shaderPath.string() +
                                     "': " + parser.getLastError());

        Shaders::ParsedShader &parsedShader = *parsedShaderOpt;

        auto compiledOpt = parser.compileAll(parsedShader, defines);
        if (!compiledOpt)
            throw std::runtime_error("Failed to compile shader '" + shaderPath.string() +
                                     "': " + parser.getLastError());

        std::vector<uint32_t> vertexSpirv, fragmentSpirv, tessCtrlSpv, tessEvalSpv;
        bool hasVertexShader = false, hasFragmentShader = false;

        for (auto &c : *compiledOpt)
        {
            switch (c.stage)
            {
            case Shaders::ShaderStage::Vertex:
                vertexSpirv = std::move(c.spirv);
                hasVertexShader = true;
                break;
            case Shaders::ShaderStage::Fragment:
                fragmentSpirv = std::move(c.spirv);
                hasFragmentShader = true;
                break;
            case Shaders::ShaderStage::TessellationControl:
                tessCtrlSpv = std::move(c.spirv);
                break;
            case Shaders::ShaderStage::TessellationEvaluation:
                tessEvalSpv = std::move(c.spirv);
                break;
            default:
                break; // Compute/Geometry entries in the same file aren't consumed here
            }
        }

        shader = Shader::CreateFromSPIRV(*logicalDevice, vertexSpirv, fragmentSpirv, tessCtrlSpv, tessEvalSpv);

        if (!hasVertexShader || !hasFragmentShader)
            throw std::runtime_error("RenderSystem pipeline requires both vertex and fragment shaders");
        if (!shader)
            throw std::runtime_error("Failed to create Vulkan shader from SPIR-V");

        stages = shader->GetPipelineStages();
    }

    void RenderPipeline::CreateDescriptorLayout()
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        const auto &descriptorBindings = shader->GetDescriptorBindings();

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.flags = pushDescriptors ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
        info.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
        info.pBindings = descriptorBindings.data();

        RenderSystem::CheckVkResult(vkCreateDescriptorSetLayout(*logicalDevice, &info, nullptr, &descriptorSetLayout));
    }

    void RenderPipeline::CreateDescriptorPool()
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        const auto &descriptorBindings = shader->GetDescriptorBindings();

        std::map<VkDescriptorType, uint32_t> typeCounts;
        for (const auto &b : descriptorBindings)
            typeCounts[b.descriptorType] += b.descriptorCount;

        typeCounts[VK_DESCRIPTOR_TYPE_SAMPLER] = std::max(typeCounts[VK_DESCRIPTOR_TYPE_SAMPLER], 16u);
        typeCounts[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE] = std::max(typeCounts[VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE], 16u);
        typeCounts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = std::max(typeCounts[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER], 16u);
        typeCounts[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE] = std::max(typeCounts[VK_DESCRIPTOR_TYPE_STORAGE_IMAGE], 16u);
        typeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] = std::max(typeCounts[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER], 16u);
        typeCounts[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER] = std::max(typeCounts[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER], 16u);
        
        std::vector<VkDescriptorPoolSize> poolSizes;
        for (const auto &[type, count] : typeCounts)
            poolSizes.push_back({type, count * 8192});

        if (poolSizes.empty())
            poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});

        VkDescriptorPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | (pushDescriptors ? 0 : VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
        info.maxSets = 8192;
        info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        info.pPoolSizes = poolSizes.data();

        RenderSystem::CheckVkResult(vkCreateDescriptorPool(*logicalDevice, &info, nullptr, &descriptorPool));
    }

    void RenderPipeline::CreateDescriptorLayout_UpdateAfterBind()
    {
        // Push descriptors and UPDATE_AFTER_BIND are mutually exclusive.
        // When pushDescriptors=true, fall back to plain layout.
        if (pushDescriptors)
        {
            CreateDescriptorLayout();
            return;
        }

        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        const auto &descriptorBindings = shader->GetDescriptorBindings();

        std::vector<VkDescriptorBindingFlags> bindingFlags(
            descriptorBindings.size(),
            VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

        VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
        flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        flagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        flagsInfo.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pNext = &flagsInfo;
        info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        info.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
        info.pBindings = descriptorBindings.data();

        RenderSystem::CheckVkResult(
            vkCreateDescriptorSetLayout(*logicalDevice, &info, nullptr, &descriptorSetLayout));
    }

    void RenderPipeline::CreatePipelineLayout()
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        const auto &pushConstants = shader->GetPushConstants();

        std::vector<VkPushConstantRange> pushConstantRanges;
        for (const auto &pc : pushConstants)
        {
            VkPushConstantRange range = {};
            range.stageFlags = pc.stageFlags;
            range.offset = pc.offset;
            range.size = pc.size;
            pushConstantRanges.push_back(range);
        }


        VkDescriptorSetLayout setLayouts[2] = {descriptorSetLayout, SharedSamplers::GetSharedSamplerSetLayout()};

        VkPipelineLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount = 2;
        info.pSetLayouts = setLayouts;
        info.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
        info.pPushConstantRanges = pushConstantRanges.data();

        RenderSystem::CheckVkResult(vkCreatePipelineLayout(*logicalDevice, &info, nullptr, &pipelineLayout));
    }

    void RenderPipeline::CreateAttributes()
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();

        if (polygonMode == VK_POLYGON_MODE_LINE &&
            !logicalDevice->GetEnabledFeatures().fillModeNonSolid)
        {
            throw std::runtime_error(
                "Cannot create RenderSystem pipeline with line polygon mode when logical device "
                "does not support non solid fills.");
        }

        inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyState.topology = topology;
        inputAssemblyState.primitiveRestartEnable = VK_FALSE;

        rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationState.depthClampEnable = VK_FALSE;
        rasterizationState.rasterizerDiscardEnable = VK_FALSE;
        rasterizationState.polygonMode = polygonMode;
        rasterizationState.cullMode = cullMode;
        rasterizationState.frontFace = frontFace;
        rasterizationState.depthBiasEnable = VK_FALSE;
        rasterizationState.lineWidth = 1.0f;

        // Pre-multiplied alpha blend:
        //   finalRGB = srcRGB*1 + dstRGB*(1-srcA)  : src RGB is already alpha-scaled
        //   finalA   = srcA*1   + dstA*(1-srcA)     : standard alpha compositing
        // This is correct for atmosphere (which outputs pre-multiplied inscatter)
        // and also works for normal opaque geometry (which outputs alpha=1, srcA=1
        // so dstRGB is zeroed : i.e. opaque geometry fully replaces background).
        blendAttachmentStates[0].blendEnable = VK_TRUE;
        blendAttachmentStates[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentStates[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentStates[0].colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentStates[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachmentStates[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentStates[0].alphaBlendOp = VK_BLEND_OP_ADD;
        blendAttachmentStates[0].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        if (isOffscreen_)
        {
            blendAttachmentStates[0].blendEnable = VK_FALSE;
        }

        colourBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colourBlendState.logicOpEnable = VK_FALSE;
        colourBlendState.logicOp = VK_LOGIC_OP_COPY;
        colourBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
        colourBlendState.pAttachments = blendAttachmentStates.data();

        depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilState.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        depthStencilState.front = depthStencilState.back;
        depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

        switch (depth)
        {
        case Depth::None:
            depthStencilState.depthTestEnable = VK_FALSE;
            depthStencilState.depthWriteEnable = VK_FALSE;
            break;
        case Depth::Read:
            depthStencilState.depthTestEnable = VK_TRUE;
            depthStencilState.depthWriteEnable = VK_FALSE;
            break;
        case Depth::Write:
            depthStencilState.depthTestEnable = VK_FALSE;
            depthStencilState.depthWriteEnable = VK_TRUE;
            break;
        case Depth::ReadWrite:
            depthStencilState.depthTestEnable = VK_TRUE;
            depthStencilState.depthWriteEnable = VK_TRUE;
            break;
        }

        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        // Sample count is set to 1x here as a safe default.
        // CreatePipeline() overrides it from the render stage once the
        // renderpass is available (IsMultisampled queries per-subpass).
        multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampleState.sampleShadingEnable = VK_FALSE;

        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
        tessellationState.patchControlPoints = 4;
    }

    void RenderPipeline::CreatePipeline()
    {
        Log::Info("Creating pipeline");
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        auto physicalDevice = RenderSystem::Get()->GetPhysicalDevice();
        auto pipelineCache = RenderSystem::Get()->GetPipelineCache();

        // Offscreen pipelines always use 1x MSAA (they render into plain images).
        // Scene pipelines query the actual render stage sample count.
        VkRenderPass vkRenderPass;
        uint32_t vkSubpass;
        if (isOffscreen_)
        {
            multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            vkRenderPass = offscreenRenderPass_;
            vkSubpass = offscreenSubpass_;
        }
        else
        {
            auto renderStage = RenderSystem::Get()->GetRenderStage(stage.first);
            multisampleState.rasterizationSamples =
                renderStage->IsMultisampled(stage.second)
                    ? physicalDevice->GetMsaaSamples()
                    : VK_SAMPLE_COUNT_1_BIT;
            vkRenderPass = *renderStage->GetRenderpass();
            vkSubpass = stage.second;
        }

        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        uint32_t lastAttribute = 0;

        for (const auto &vertexInput : vertexInputs)
        {
            for (const auto &binding : vertexInput.GetBindingDescriptions())
                bindingDescriptions.emplace_back(binding);

            for (const auto &attribute : vertexInput.GetAttributeDescriptions())
            {
                auto &a = attributeDescriptions.emplace_back(attribute);
                a.location += lastAttribute;
            }

            if (!vertexInput.GetAttributeDescriptions().empty())
                lastAttribute = attributeDescriptions.back().location + 1;
        }

        vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputStateCreateInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(stages.size());
        pipelineCreateInfo.pStages = stages.data();
        pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pTessellationState = &tessellationState;
        pipelineCreateInfo.pViewportState = &viewportState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pMultisampleState = &multisampleState;
        pipelineCreateInfo.pDepthStencilState = &depthStencilState;
        pipelineCreateInfo.pColorBlendState = &colourBlendState;
        pipelineCreateInfo.pDynamicState = &dynamicState;
        pipelineCreateInfo.layout = pipelineLayout;
        pipelineCreateInfo.renderPass = vkRenderPass;
        pipelineCreateInfo.subpass = vkSubpass;
        pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineCreateInfo.basePipelineIndex = -1;

        auto result = vkCreateGraphicsPipelines(*logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline);
        if (result != VK_SUCCESS)
            throw std::runtime_error("vkCreateGraphicsPipelines failed: " + RenderSystem::StrVkResult(result));
    }

    void RenderPipeline::CreatePipelinePolygon()
    {
        CreatePipeline();
    }

    void RenderPipeline::CreatePipelineMrt()
    {
        auto renderStage = RenderSystem::Get()->GetRenderStage(stage.first);
        auto attachmentCount = renderStage->GetAttachmentCount(stage.second);

        std::vector<VkPipelineColorBlendAttachmentState> blendStates;
        blendStates.reserve(attachmentCount);

        for (uint32_t i = 0; i < attachmentCount; i++)
        {
            VkPipelineColorBlendAttachmentState s = {};
            s.blendEnable = VK_TRUE;
            s.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            s.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            s.colorBlendOp = VK_BLEND_OP_ADD;
            s.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            s.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            s.alphaBlendOp = VK_BLEND_OP_ADD;
            s.colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            blendStates.emplace_back(s);
        }

        colourBlendState.attachmentCount = static_cast<uint32_t>(blendStates.size());
        colourBlendState.pAttachments = blendStates.data();

        CreatePipeline();
    }
}
