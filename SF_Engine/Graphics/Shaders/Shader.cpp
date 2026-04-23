#include "Shader.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace SF::Engine
{
    Shader::Shader(VkDevice device) : device(device) {}

    Shader::~Shader()
    {
        Cleanup();
    }

    void Shader::Cleanup()
    {
        for (const auto &info : moduleInfos)
        {
            if (info.module != VK_NULL_HANDLE)
            {
                vkDestroyShaderModule(device, info.module, nullptr);
            }
        }
        moduleInfos.clear();
    }

    std::shared_ptr<Shader> Shader::CreateFromSPIRV(VkDevice device,
                                                    const std::vector<uint32_t> &vertSpirv,
                                                    const std::vector<uint32_t> &fragSpirv)
    {
        auto shader = std::shared_ptr<Shader>(new Shader(device));

        shader->AddShaderModule(vertSpirv, VK_SHADER_STAGE_VERTEX_BIT);
        shader->AddShaderModule(fragSpirv, VK_SHADER_STAGE_FRAGMENT_BIT);

        shader->ReflectFromSPIRV(vertSpirv, VK_SHADER_STAGE_VERTEX_BIT);
        shader->ReflectFromSPIRV(fragSpirv, VK_SHADER_STAGE_FRAGMENT_BIT);

        return shader;
    }

    std::shared_ptr<Shader> Shader::CreateComputeFromSPIRV(
        VkDevice device, const std::vector<uint32_t> &computeSpirv)
    {
        auto shader = std::shared_ptr<Shader>(new Shader(device));

        shader->AddShaderModule(computeSpirv, VK_SHADER_STAGE_COMPUTE_BIT);
        shader->ReflectFromSPIRV(computeSpirv, VK_SHADER_STAGE_COMPUTE_BIT);

        return shader;
    }

    std::shared_ptr<Shader> Shader::CreateFromFile(VkDevice device,
                                                   const std::filesystem::path &vertPath,
                                                   const std::filesystem::path &fragPath)
    {
        auto readFile = [](const std::filesystem::path &path) -> std::vector<uint32_t>
        {
            std::ifstream file(path, std::ios::ate | std::ios::binary);
            if (!file.is_open())
            {
                std::cerr << "Failed to open shader file: " << path << std::endl;
                return {};
            }

            size_t fileSize = file.tellg();
            std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
            file.seekg(0);
            file.read(reinterpret_cast<char *>(buffer.data()), fileSize);
            file.close();
            return buffer;
        };

        auto vertSpirv = readFile(vertPath);
        auto fragSpirv = readFile(fragPath);

        if (vertSpirv.empty() || fragSpirv.empty())
            return nullptr;

        return CreateFromSPIRV(device, vertSpirv, fragSpirv);
    }

    void Shader::AddShaderModule(const std::vector<uint32_t> &spirv, VkShaderStageFlagBits stage)
    {
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = spirv.size() * sizeof(uint32_t);
        info.pCode = spirv.data();

        VkShaderModule module;
        VkResult result = vkCreateShaderModule(device, &info, nullptr, &module);

        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create shader module! VkResult: " << result << std::endl;
            return;
        }

        moduleInfos.push_back({module, stage});
    }

    void Shader::ReflectFromSPIRV(const std::vector<uint32_t> &spirv, VkShaderStageFlagBits stage)
    {
        SpvReflectShaderModule reflectModule;
        SpvReflectResult result = spvReflectCreateShaderModule(spirv.size() * sizeof(uint32_t),
                                                               spirv.data(), &reflectModule);

        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            std::cerr << "SPIRV-Reflect failed to create shader module! Error: " << result
                      << std::endl;
            return;
        }

        // Reflect descriptor bindings
        uint32_t count = 0;
        spvReflectEnumerateDescriptorBindings(&reflectModule, &count, nullptr);

        if (count > 0)
        {
            std::vector<SpvReflectDescriptorBinding *> bindings(count);
            spvReflectEnumerateDescriptorBindings(&reflectModule, &count, bindings.data());

            std::map<VkDescriptorType, uint32_t> descriptorPoolCounts;

            for (auto *binding : bindings)
            {
                VkDescriptorType type = static_cast<VkDescriptorType>(binding->descriptor_type);

                // Store uniform info
                UniformInfo info{.binding = binding->binding,
                                 .offset = 0,
                                 .size = binding->block.size,
                                 .type = type,
                                 .stageFlags = static_cast<VkShaderStageFlags>(stage),
                                 .readOnly = (binding->decoration_flags &
                                              SPV_REFLECT_DECORATION_NON_WRITABLE) != 0,
                                 .writeOnly = (binding->decoration_flags &
                                               SPV_REFLECT_DECORATION_NON_READABLE) != 0};

                auto it = uniforms.find(binding->name);
                if (it != uniforms.end())
                {
                    // Merge stage flags if binding already exists
                    it->second.stageFlags |= stage;
                }
                else
                {
                    uniforms[binding->name] = info;
                    descriptorLocations[binding->name] = binding->binding;
                    descriptorSizes[binding->name] = binding->block.size;
                    descriptorTypes[binding->binding] = type;
                }

                // Update last descriptor binding
                lastDescriptorBinding = std::max(lastDescriptorBinding, binding->binding);

                // Create descriptor set layout binding
                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = binding->binding;
                layoutBinding.descriptorType = type;
                layoutBinding.descriptorCount = binding->count;
                layoutBinding.stageFlags = static_cast<VkShaderStageFlags>(stage);
                layoutBinding.pImmutableSamplers = nullptr;

                // Check if binding already exists (from another stage)
                auto bindingIt = std::find_if(descriptorBindings.begin(), descriptorBindings.end(),
                                              [&](const VkDescriptorSetLayoutBinding &b)
                                              { return b.binding == layoutBinding.binding; });

                if (bindingIt != descriptorBindings.end())
                {
                    // Merge stage flags
                    bindingIt->stageFlags |= static_cast<VkShaderStageFlags>(stage);
                }
                else
                {
                    descriptorBindings.push_back(layoutBinding);
                    IncrementDescriptorPool(descriptorPoolCounts, type);
                }

                // Load uniform block if it's a UBO or SSBO
                if (binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                    binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                {
                    LoadUniformBlock(binding->block, stage);
                }
            }

            // Convert descriptor pool counts to VkDescriptorPoolSize
            for (const auto &[type, count] : descriptorPoolCounts)
            {
                auto poolIt =
                    std::find_if(descriptorPools.begin(), descriptorPools.end(),
                                 [type](const VkDescriptorPoolSize &p)
                                 { return p.type == type; });

                if (poolIt != descriptorPools.end())
                {
                    poolIt->descriptorCount += count;
                }
                else
                {
                    descriptorPools.push_back({type, count});
                }
            }
        }

        // Reflect push constants
        count = 0;
        spvReflectEnumeratePushConstantBlocks(&reflectModule, &count, nullptr);

        if (count > 0)
        {
            std::vector<SpvReflectBlockVariable *> blocks(count);
            spvReflectEnumeratePushConstantBlocks(&reflectModule, &count, blocks.data());

            for (auto *block : blocks)
            {
                // Check if this push constant range already exists
                auto it = std::find_if(
                    pushConstants.begin(), pushConstants.end(), [&](const PushConstantRange &r)
                    { return r.offset == block->offset && r.size == block->size; });

                if (it != pushConstants.end())
                {
                    // Merge stage flags for identical range
                    it->stageFlags |= static_cast<VkShaderStageFlags>(stage);
                }
                else
                {
                    PushConstantRange range{.offset = block->offset,
                                            .size = block->size,
                                            .stageFlags = static_cast<VkShaderStageFlags>(stage)};
                    pushConstants.push_back(range);
                }
            }
        }

        // Reflect input attributes (vertex shader only)
        if (stage == VK_SHADER_STAGE_VERTEX_BIT)
        {
            count = 0;
            spvReflectEnumerateInputVariables(&reflectModule, &count, nullptr);

            if (count > 0)
            {
                std::vector<SpvReflectInterfaceVariable *> inputVars(count);
                spvReflectEnumerateInputVariables(&reflectModule, &count, inputVars.data());

                for (auto *var : inputVars)
                {
                    if (var->built_in != -1) // Skip built-ins
                        continue;

                    Attribute attr(0, var->location, var->numeric.scalar.width / 8,
                                   static_cast<VkFormat>(var->format));
                    attributes[var->name] = attr;

                    VkVertexInputAttributeDescription attrDesc{};
                    attrDesc.location = var->location;
                    attrDesc.binding = 0; // Default binding
                    attrDesc.format = static_cast<VkFormat>(var->format);
                    attrDesc.offset = 0; // Will be calculated based on layout
                    attributeDescriptions.push_back(attrDesc);
                }
            }
        }

        spvReflectDestroyShaderModule(&reflectModule);
    }

    void Shader::LoadUniformBlock(const SpvReflectBlockVariable &block, VkShaderStageFlagBits stage)
    {
        UniformBlock uniformBlock;
        uniformBlock.binding = -1; // Will be set by descriptor binding
        uniformBlock.size = block.size;
        uniformBlock.stageFlags = static_cast<VkShaderStageFlags>(stage); // MSVC BS DETECTED
        uniformBlock.type = UniformBlock::Type::Uniform;

        // Load members
        for (uint32_t i = 0; i < block.member_count; ++i)
        {
            const auto &member = block.members[i];
            UniformInfo memberInfo{.binding = 0,
                                   .offset = member.offset,
                                   .size = member.size,
                                   .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                   .stageFlags = static_cast<VkShaderStageFlags>(stage),
                                   .readOnly = false,
                                   .writeOnly = false};
            uniformBlock.uniforms[member.name] = memberInfo;
        }

        uniformBlocks[block.name] = uniformBlock;
    }

    void Shader::IncrementDescriptorPool(std::map<VkDescriptorType, uint32_t> &poolCounts,
                                         VkDescriptorType type)
    {
        poolCounts[type]++;
    }

    std::vector<VkPipelineShaderStageCreateInfo> Shader::GetPipelineStages() const
    {
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        stages.reserve(moduleInfos.size());

        for (const auto &info : moduleInfos)
        {
            VkPipelineShaderStageCreateInfo stageInfo{};
            stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage = info.stage;
            stageInfo.module = info.module;
            stageInfo.pName = "main";
            stageInfo.pSpecializationInfo = nullptr;
            stages.push_back(stageInfo);
        }

        return stages;
    }

    VkDescriptorSetLayout Shader::CreateDescriptorSetLayout() const
    {
        if (descriptorBindings.empty())
            return VK_NULL_HANDLE;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
        layoutInfo.pBindings = descriptorBindings.data();

        VkDescriptorSetLayout layout;
        VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout);

        if (result != VK_SUCCESS)
        {
            std::cerr << "Failed to create descriptor set layout! Error: " << result << std::endl;
            return VK_NULL_HANDLE;
        }

        return layout;
    }

    std::vector<VkPushConstantRange> Shader::GetPushConstantRanges() const
    {
        std::vector<VkPushConstantRange> ranges;
        ranges.reserve(pushConstants.size());

        for (const auto &pc : pushConstants)
        {
            VkPushConstantRange range{};
            range.stageFlags = pc.stageFlags;
            range.offset = pc.offset;
            range.size = pc.size;
            ranges.push_back(range);
        }

        return ranges;
    }

    std::optional<uint32_t> Shader::GetDescriptorLocation(const std::string &name) const
    {
        auto it = descriptorLocations.find(name);
        if (it != descriptorLocations.end())
            return it->second;
        return std::nullopt;
    }

    std::optional<uint32_t> Shader::GetDescriptorSize(const std::string &name) const
    {
        auto it = descriptorSizes.find(name);
        if (it != descriptorSizes.end())
            return it->second;
        return std::nullopt;
    }

    std::optional<UniformInfo> Shader::GetUniform(const std::string &name) const
    {
        auto it = uniforms.find(name);
        if (it != uniforms.end())
            return it->second;
        return std::nullopt;
    }

    std::optional<Shader::UniformBlock> Shader::GetUniformBlock(const std::string &name) const
    {
        auto it = uniformBlocks.find(name);
        if (it != uniformBlocks.end())
            return it->second;
        return std::nullopt;
    }

    std::optional<Shader::Attribute> Shader::GetAttribute(const std::string &name) const
    {
        auto it = attributes.find(name);
        if (it != attributes.end())
            return it->second;
        return std::nullopt;
    }

    std::optional<VkDescriptorType> Shader::GetDescriptorType(uint32_t location) const
    {
        auto it = descriptorTypes.find(location);
        if (it != descriptorTypes.end())
            return it->second;
        return std::nullopt;
    }

    bool Shader::HasStage(VkShaderStageFlagBits stage) const
    {
        return std::any_of(moduleInfos.begin(), moduleInfos.end(),
                           [stage](const ModuleInfo &info)
                           { return info.stage == stage; });
    }

    void Shader::Reload(const std::vector<uint32_t> &newSpirv, VkShaderStageFlagBits stage)
    {
        for (auto &info : moduleInfos)
        {
            if (info.stage == stage)
            {
                vkDestroyShaderModule(device, info.module, nullptr);

                VkShaderModuleCreateInfo createInfo{};
                createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createInfo.codeSize = newSpirv.size() * sizeof(uint32_t);
                createInfo.pCode = newSpirv.data();

                VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &info.module);

                if (result != VK_SUCCESS)
                {
                    std::cerr << "Failed to reload shader module! Error: " << result << std::endl;
                    return;
                }

                // Clear and rebuild reflection data
                uniforms.clear();
                uniformBlocks.clear();
                attributes.clear();
                pushConstants.clear();
                descriptorBindings.clear();
                descriptorPools.clear();
                descriptorLocations.clear();
                descriptorSizes.clear();
                descriptorTypes.clear();
                lastDescriptorBinding = 0;

                // Re-reflect all stages
                for (const auto &moduleInfo : moduleInfos)
                {
                    // Note: You'll need to store original SPIRV to re-reflect properly
                    // This is a simplified version
                    ReflectFromSPIRV(newSpirv, moduleInfo.stage);
                }

                std::cout << "Shader stage reloaded successfully!" << std::endl;
                break;
            }
        }
    }

    VkFormat Shader::GlTypeToVk(int32_t type)
    {
        // Map common GL types to Vulkan formats
        switch (type)
        {
        case 0x1406:
            return VK_FORMAT_R32_SFLOAT; // GL_FLOAT
        case 0x8B50:
            return VK_FORMAT_R32G32_SFLOAT; // GL_FLOAT_VEC2
        case 0x8B51:
            return VK_FORMAT_R32G32B32_SFLOAT; // GL_FLOAT_VEC3
        case 0x8B52:
            return VK_FORMAT_R32G32B32A32_SFLOAT; // GL_FLOAT_VEC4
        case 0x1404:
            return VK_FORMAT_R32_SINT; // GL_INT
        case 0x8B53:
            return VK_FORMAT_R32G32_SINT; // GL_INT_VEC2
        case 0x8B54:
            return VK_FORMAT_R32G32B32_SINT; // GL_INT_VEC3
        case 0x8B55:
            return VK_FORMAT_R32G32B32A32_SINT; // GL_INT_VEC4
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }
}