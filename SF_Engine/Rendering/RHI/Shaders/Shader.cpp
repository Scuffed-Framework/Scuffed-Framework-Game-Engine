#include "Shader.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace SF::Engine
{
    Shader::Shader(VkDevice device) : device(device) {}

    Shader::~Shader() { Cleanup(); }

    void Shader::Cleanup()
    {
        for (const auto &info: moduleInfos)
        {
            if (info.module != VK_NULL_HANDLE)
            {
                vkDestroyShaderModule(device, info.module, nullptr);
            }
        }
        moduleInfos.clear();
    }

    std::shared_ptr<Shader> Shader::CreateFromStages(VkDevice device, const std::vector<StageSpirv> &stages)
    {
        // Canonical graphics pipeline order. Vulkan doesn't care, but a stable order keeps
        // descriptor/push-constant reflection merging deterministic.
        static constexpr VkShaderStageFlagBits ORDER[] = {VK_SHADER_STAGE_VERTEX_BIT,
                                                          VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                                                          VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                                                          VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};

        auto shader = std::shared_ptr<Shader>(new Shader(device));

        size_t consumed = 0;
        for (auto stage: ORDER)
        {
            const StageSpirv *found = nullptr;
            for (const auto &s: stages)
            {
                if (s.stage != stage)
                    continue;
                if (found)
                {
                    std::cerr << "Shader::CreateFromStages: duplicate stage " << stage << std::endl;
                    return nullptr;
                }
                found = &s;
            }
            if (!found)
                continue;
            if (found->spirv.empty())
            {
                std::cerr << "Shader::CreateFromStages: empty SPIR-V for stage " << stage << std::endl;
                return nullptr;
            }

            const size_t before = shader->moduleInfos.size();
            shader->AddShaderModule(found->spirv, stage);
            if (shader->moduleInfos.size() == before)
                return nullptr; // module creation failed (already logged)
            shader->ReflectFromSPIRV(found->spirv, stage);
            ++consumed;
        }

        if (consumed != stages.size())
        {
            std::cerr << "Shader::CreateFromStages: unsupported stage in graphics shader" << std::endl;
            return nullptr;
        }
        return shader;
    }

    std::shared_ptr<Shader> Shader::CreateFromSPIRV(VkDevice device, const std::vector<uint32_t> &vertSpirv,
                                                    const std::vector<uint32_t> &fragSpirv,
                                                    const std::vector<uint32_t> &tessCtrlSpirv,
                                                    const std::vector<uint32_t> &tessEvalSpirv,
                                                    const std::vector<uint32_t> &geomSpirv)
    {
        std::vector<StageSpirv> stages;
        stages.push_back({VK_SHADER_STAGE_VERTEX_BIT, vertSpirv});
        if (!tessCtrlSpirv.empty())
            stages.push_back({VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, tessCtrlSpirv});
        if (!tessEvalSpirv.empty())
            stages.push_back({VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, tessEvalSpirv});
        if (!geomSpirv.empty())
            stages.push_back({VK_SHADER_STAGE_GEOMETRY_BIT, geomSpirv});
        stages.push_back({VK_SHADER_STAGE_FRAGMENT_BIT, fragSpirv});
        return CreateFromStages(device, stages);
    }

    std::shared_ptr<Shader> Shader::CreateComputeFromSPIRV(VkDevice device, const std::vector<uint32_t> &computeSpirv)
    {
        auto shader = std::shared_ptr<Shader>(new Shader(device));

        shader->AddShaderModule(computeSpirv, VK_SHADER_STAGE_COMPUTE_BIT);
        shader->ReflectFromSPIRV(computeSpirv, VK_SHADER_STAGE_COMPUTE_BIT);

        return shader;
    }

    std::shared_ptr<Shader> Shader::CreateFromFile(VkDevice device, const std::filesystem::path &vertPath,
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
            file.read(reinterpret_cast<char *>(buffer.data()), static_cast<signed long>(fileSize));
            file.close();
            return buffer;
        };

        const auto vertSpirv = readFile(vertPath);
        const auto fragSpirv = readFile(fragPath);

        if (vertSpirv.empty() || fragSpirv.empty())
            return nullptr;

        return CreateFromSPIRV(device, vertSpirv, fragSpirv);
    }

    void Shader::AddShaderModule(const std::vector<uint32_t> &spirv, VkShaderStageFlagBits stage)
    {
        VkShaderModuleCreateInfo info{};
        info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = spirv.size() * sizeof(uint32_t);
        info.pCode    = spirv.data();

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
        SpvReflectResult result =
                spvReflectCreateShaderModule(spirv.size() * sizeof(uint32_t), spirv.data(), &reflectModule);

        if (result != SPV_REFLECT_RESULT_SUCCESS)
        {
            std::cerr << "SPIRV-Reflect failed to create shader module! Error: " << result << std::endl;
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

            for (auto *binding: bindings)
            {
                auto type = static_cast<VkDescriptorType>(binding->descriptor_type);

                // Store uniform info
                UniformInfo info{.binding    = binding->binding,
                                 .offset     = 0,
                                 .size       = binding->block.size,
                                 .type       = type,
                                 .stageFlags = static_cast<VkShaderStageFlags>(stage),
                                 .readOnly   = (binding->decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE) != 0,
                                 .writeOnly  = (binding->decoration_flags & SPV_REFLECT_DECORATION_NON_READABLE) != 0};

                auto it = uniforms.find(binding->name);
                if (it != uniforms.end())
                {
                    // Merge stage flags if binding already exists
                    it->second.stageFlags |= stage;
                } else
                {
                    uniforms[binding->name]            = info;
                    descriptorLocations[binding->name] = binding->binding;
                    descriptorSizes[binding->name]     = binding->block.size;
                    descriptorTypes[binding->binding]  = type;
                }

                // Update last descriptor binding
                lastDescriptorBinding = std::max(lastDescriptorBinding, binding->binding);

                // Create descriptor set layout binding
                VkDescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding            = binding->binding;
                layoutBinding.descriptorType     = type;
                layoutBinding.descriptorCount    = binding->count;
                layoutBinding.stageFlags         = static_cast<VkShaderStageFlags>(stage);
                layoutBinding.pImmutableSamplers = nullptr;

                // Check if binding already exists (from another stage)
                auto bindingIt = ranges::find_if(descriptorBindings, [&](const VkDescriptorSetLayoutBinding &b)
                                                 { return b.binding == layoutBinding.binding; });

                if (bindingIt != descriptorBindings.end())
                {
                    // Merge stage flags
                    bindingIt->stageFlags |= static_cast<VkShaderStageFlags>(stage);
                } else
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
            for (const auto &[type, descCount]: descriptorPoolCounts)
            {
                auto poolIt = ranges::find_if(descriptorPools,
                                              [type](const VkDescriptorPoolSize &p) { return p.type == type; });

                if (poolIt != descriptorPools.end())
                {
                    poolIt->descriptorCount += descCount;
                } else
                {
                    descriptorPools.push_back({.type = type, .descriptorCount = descCount});
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

            for (auto *block: blocks)
            {
                // Check if this push constant range already exists
                auto it = ranges::find_if(pushConstants, [&](const PushConstantRange &r)
                                          { return r.offset == block->offset && r.size == block->size; });

                if (it != pushConstants.end())
                {
                    // Merge stage flags for identical range
                    it->stageFlags |= static_cast<VkShaderStageFlags>(stage);
                } else
                {
                    PushConstantRange range{.offset     = block->offset,
                                            .size       = block->size,
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

                for (auto *var: inputVars)
                {
                    if (var->built_in != -1) // Skip built-ins
                        continue;

                    Attribute attr(0, static_cast<int32_t>(var->location),
                                   static_cast<int32_t>(var->numeric.scalar.width) / 8,
                                   static_cast<VkFormat>(var->format));
                    attributes[var->name] = attr;

                    VkVertexInputAttributeDescription attrDesc{};
                    attrDesc.location = var->location;
                    attrDesc.binding  = 0; // Default binding
                    attrDesc.format   = static_cast<VkFormat>(var->format);
                    attrDesc.offset   = 0; // Will be calculated based on layout
                    attributeDescriptions.push_back(attrDesc);
                }
            }
        }

        spvReflectDestroyShaderModule(&reflectModule);
    }

    void Shader::LoadUniformBlock(const SpvReflectBlockVariable &block, VkShaderStageFlagBits stage)
    {
        UniformBlock uniformBlock;
        uniformBlock.binding    = -1; // Will be set by descriptor binding
        uniformBlock.size       = static_cast<int32_t>(block.size);
        uniformBlock.stageFlags = static_cast<VkShaderStageFlags>(stage); // MSVC BS DETECTED
        uniformBlock.type       = UniformBlock::Type::Uniform;

        // Load members
        for (uint32_t i = 0; i < block.member_count; ++i)
        {
            const auto &member = block.members[i];
            UniformInfo memberInfo{.binding    = 0,
                                   .offset     = member.offset,
                                   .size       = member.size,
                                   .type       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                   .stageFlags = static_cast<VkShaderStageFlags>(stage),
                                   .readOnly   = false,
                                   .writeOnly  = false};
            uniformBlock.uniforms[member.name] = memberInfo;
        }

        uniformBlocks[block.name] = uniformBlock;
    }

    void Shader::IncrementDescriptorPool(std::map<VkDescriptorType, uint32_t> &poolCounts, VkDescriptorType type)
    {
        poolCounts[type]++;
    }

    std::vector<VkPipelineShaderStageCreateInfo> Shader::GetPipelineStages() const
    {
        std::vector<VkPipelineShaderStageCreateInfo> stages;
        stages.reserve(moduleInfos.size());

        for (const auto &info: moduleInfos)
        {
            VkPipelineShaderStageCreateInfo stageInfo{};
            stageInfo.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage               = info.stage;
            stageInfo.module              = info.module;
            stageInfo.pName               = "main";
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
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
        layoutInfo.pBindings    = descriptorBindings.data();

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

        for (const auto &pc: pushConstants)
        {
            VkPushConstantRange range{};
            range.stageFlags = pc.stageFlags;
            range.offset     = pc.offset;
            range.size       = pc.size;
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
        return ranges::any_of(moduleInfos, [stage](const ModuleInfo &info) { return info.stage == stage; });
    }

    void Shader::Reload(const std::vector<uint32_t> &newSpirv, VkShaderStageFlagBits stage)
    {
        for (auto &info: moduleInfos)
        {
            if (info.stage == stage)
            {
                vkDestroyShaderModule(device, info.module, nullptr);

                VkShaderModuleCreateInfo createInfo{};
                createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                createInfo.codeSize = newSpirv.size() * sizeof(uint32_t);
                createInfo.pCode    = newSpirv.data();

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
                for (const auto &moduleInfo: moduleInfos)
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
} // namespace SF::Engine
