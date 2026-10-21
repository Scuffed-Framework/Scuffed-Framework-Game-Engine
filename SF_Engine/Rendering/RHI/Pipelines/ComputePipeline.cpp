#include "ComputePipeline.hpp"

#include <Engine/Engine.hpp>
#include <Engine/Log/Log.hpp>
#include <Rendering/RHI/Shaders/Parser/Parser.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Rendering/SharedSamplers.hpp>
#include <stdexcept>

namespace SF::Engine
{
    ComputePipeline::ComputePipeline(std::filesystem::path shaderStage, std::vector<Shader::Define> defines,
                                     bool pushDescriptors, std::vector<VkDescriptorSetLayout> additionalLayouts,
                                     UVec3 localSize) :
        shaderStage(std::move(shaderStage)), defines(std::move(defines)), pushDescriptors(pushDescriptors),
        additionalLayouts(std::move(additionalLayouts)), localSize(localSize)
    {
        if (localSize.x == 0 || localSize.y == 0 || localSize.z == 0)
            throw std::runtime_error("ComputePipeline: localSize components must be non-zero");

        device_ = *RenderSystem::Get()->GetLogicalDevice();
        CreateShaderProgram("");
        CreateDescriptorLayout();
        CreateDescriptorPool();
        CreatePipelineLayout();
        CreatePipelineCompute();
    }

    ComputePipeline::ComputePipeline(std::filesystem::path shaderStage, std::string entry,
                                     std::vector<Shader::Define> defines, bool pushDescriptors,
                                     std::vector<VkDescriptorSetLayout> additionalLayouts, UVec3 localSize) :
        shaderStage(std::move(shaderStage)), defines(std::move(defines)), pushDescriptors(pushDescriptors),
        additionalLayouts(std::move(additionalLayouts)), localSize(localSize), entryOpt(entry)
    {
        if (localSize.x == 0 || localSize.y == 0 || localSize.z == 0)
            throw std::runtime_error("ComputePipeline: localSize components must be non-zero");

        device_ = *RenderSystem::Get()->GetLogicalDevice();
        CreateShaderProgram(entry);
        CreateDescriptorLayout();
        CreateDescriptorPool();
        CreatePipelineLayout();
        CreatePipelineCompute();
    }


    ComputePipeline::~ComputePipeline()
    {
        // Use stored device handle : RenderSystem::Get() may be dead during shutdown.
        if (device_ == VK_NULL_HANDLE)
            return;
        vkDestroyPipeline(device_, pipeline, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(device_, descriptorPool, nullptr);
    }

    void ComputePipeline::CreateShaderProgram(std::string entry)
    {
        auto *dev = RenderSystem::Get()->GetLogicalDevice();

        Shaders::ShaderParser parser;
        Log::Info("Loading compute shader: {} (cwd={})", shaderStage.string(), GetExecutablePath().string());

        auto parsedOpt = parser.parse(std::filesystem::path(GetExecutablePath() / shaderStage).string());
        if (!parsedOpt)
            throw std::runtime_error("Failed to parse compute shader '" + shaderStage.string() +
                                     "': " + parser.getLastError());

        std::optional<Shaders::CompiledShader> compiledOpt;
        if (!entry.empty())
            compiledOpt = parser.compile(*parsedOpt, Shaders::ShaderStage::Compute, defines, entry);
        else
            compiledOpt = parser.compile(*parsedOpt, Shaders::ShaderStage::Compute, defines);

        if (!compiledOpt)
            throw std::runtime_error("Failed to compile compute shader '" + shaderStage.string() +
                                     "': " + parser.getLastError());

        shader = Shader::CreateComputeFromSPIRV(*dev, compiledOpt->spirv);
        if (!shader)
            throw std::runtime_error("Failed to create Vulkan compute shader from SPIR-V");
    }

    void ComputePipeline::CreateDescriptorLayout()
    {
        auto *dev            = RenderSystem::Get()->GetLogicalDevice();
        const auto &bindings = shader->GetDescriptorBindings();

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.flags        = pushDescriptors ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR : 0;
        info.bindingCount = static_cast<uint32_t>(bindings.size());
        info.pBindings    = bindings.data();

        RenderSystem::CheckVkResult(vkCreateDescriptorSetLayout(*dev, &info, nullptr, &descriptorSetLayout));
    }

    void ComputePipeline::CreateDescriptorPool()
    {
        auto *dev            = RenderSystem::Get()->GetLogicalDevice();
        const auto &bindings = shader->GetDescriptorBindings();

        std::map<VkDescriptorType, uint32_t> counts;
        for (const auto &b: bindings)
            counts[b.descriptorType] += b.descriptorCount;

        std::vector<VkDescriptorPoolSize> sizes;
        for (const auto &[type, count]: counts)
            sizes.push_back({type, count * 64});

        if (sizes.empty())
            sizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1});

        VkDescriptorPoolCreateInfo info{};
        info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        info.maxSets       = 64;
        info.poolSizeCount = static_cast<uint32_t>(sizes.size());
        info.pPoolSizes    = sizes.data();

        RenderSystem::CheckVkResult(vkCreateDescriptorPool(*dev, &info, nullptr, &descriptorPool));
    }

    void ComputePipeline::CreatePipelineLayout()
    {
        auto *dev       = RenderSystem::Get()->GetLogicalDevice();
        const auto &pcs = shader->GetPushConstants();

        std::vector<VkPushConstantRange> ranges;
        for (const auto &pc: pcs)
            ranges.push_back({pc.stageFlags, pc.offset, pc.size});

        std::vector<VkDescriptorSetLayout> setLayouts;
        setLayouts.reserve(2 + additionalLayouts.size());
        setLayouts.push_back(descriptorSetLayout);
        setLayouts.push_back(SharedSamplers::GetSharedSamplerSetLayout());
        setLayouts.insert(setLayouts.end(), additionalLayouts.begin(), additionalLayouts.end());

        VkPipelineLayoutCreateInfo info{};
        info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
        info.pSetLayouts            = setLayouts.data();
        info.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
        info.pPushConstantRanges    = ranges.data();

        RenderSystem::CheckVkResult(vkCreatePipelineLayout(*dev, &info, nullptr, &pipelineLayout));
    }

    void ComputePipeline::CreatePipelineCompute()
    {
        auto *dev  = RenderSystem::Get()->GetLogicalDevice();
        auto cache = RenderSystem::Get()->GetPipelineCache();

        const auto &stages = shader->GetPipelineStages();
        if (stages.empty())
            throw std::runtime_error("Compute shader has no pipeline stages after reflection");

        VkComputePipelineCreateInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        info.stage  = stages[0];
        info.layout = pipelineLayout;

        Log::Info("Creating compute pipeline for: {}", shaderStage.string());
        RenderSystem::CheckVkResult(vkCreateComputePipelines(*dev, cache, 1, &info, nullptr, &pipeline));
        Log::Info("Compute pipeline created successfully");
    }

    void ComputePipeline::Dispatch(const CommandBuffer &commandBuffer, const UVec3 &extent,
                                    const UVec3 &localSize) const
    {
        const UVec3 groupCount{Mathematics::CeilDiv(extent.x, localSize.x), Mathematics::CeilDiv(extent.y, localSize.y),
                               Mathematics::CeilDiv(extent.z, localSize.z)};

        vkCmdDispatch(commandBuffer, groupCount.x, groupCount.y, groupCount.z);
    }

    void ComputePipeline::ReloadShader(const std::vector<uint32_t> &newSpirv)
    {
        auto *dev = RenderSystem::Get()->GetLogicalDevice();
        vkDeviceWaitIdle(*dev);
        shader->Reload(newSpirv, VK_SHADER_STAGE_COMPUTE_BIT);
    }
} // namespace SF::Engine
