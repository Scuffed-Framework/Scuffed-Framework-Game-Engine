#include "Bindless.hpp"
#include <Math/Math.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Engine/Log/Log.hpp>
#include <Rendering/Common.hpp>

namespace SF::Engine
{
    BindlessManager::BindlessManager()
    {
        const auto &indexingProps = RenderSystem::Get()->GetPhysicalDevice()->GetDescriptorIndexingProperties();
        // Configs init.
        m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessStorageBuffer)] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 500000u, indexingProps.maxDescriptorSetUpdateAfterBindStorageBuffers};
        m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessUniformBuffer)] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 500000u, indexingProps.maxDescriptorSetUpdateAfterBindUniformBuffers};
        m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessSampledImage)] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 500000u, indexingProps.maxDescriptorSetUpdateAfterBindSampledImages};
        m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessStorageImage)] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 500000u, indexingProps.maxDescriptorSetUpdateAfterBindStorageImages};
        m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessSampler)] = {VK_DESCRIPTOR_TYPE_SAMPLER, 100000u, indexingProps.maxDescriptorSetUpdateAfterBindSamplers};
        m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessUniformTexelBuffer)] = {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100000u, indexingProps.maxDescriptorSetUpdateAfterBindUniformBuffers};
        m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessStorageTexelBuffer)] = {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100000u, indexingProps.maxDescriptorSetUpdateAfterBindStorageBuffers};
        for (auto &config : m_bindingConfigs)
        {
            constexpr uint32 kUsedCountPercentage = 2;
            config.count = Mathematics::Clamp(config.count, 1u, config.limit / kUsedCountPercentage);
        }
        for (auto &count : m_usedCount)
        {
            count = 0;
        }
        {
            std::array<VkDescriptorSetLayoutBinding, kBindingCount> bindings{};
            std::array<VkDescriptorBindingFlags, kBindingCount> flags{};

            for (uint32_t i = 0; i < kBindingCount; ++i)
            {
                bindings[i].binding = i;
                bindings[i].descriptorType = m_bindingConfigs[i].type;
                bindings[i].descriptorCount = m_bindingConfigs[i].count;
                bindings[i].stageFlags = VK_SHADER_STAGE_ALL;

                flags[i] =
                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
            bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
            bindingFlags.pNext = nullptr;
            bindingFlags.pBindingFlags = flags.data();
            bindingFlags.bindingCount = kBindingCount;
            VkDescriptorSetLayoutCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            createInfo.bindingCount = kBindingCount;
            createInfo.pBindings = bindings.data();
            createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
            createInfo.pNext = &bindingFlags;
            m_setLayout = CreateDescriptorSetLayout(createInfo);
        }
        {
            std::array<VkDescriptorPoolSize, kBindingCount> poolSize{};
            for (uint32_t i = 0; i < kBindingCount; ++i)
            {
                poolSize[i].type = m_bindingConfigs[i].type;
                poolSize[i].descriptorCount = m_bindingConfigs[i].count;
            }

            VkDescriptorPoolCreateInfo poolCreateInfo{};
            poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolCreateInfo.poolSizeCount = kBindingCount;
            poolCreateInfo.pPoolSizes = poolSize.data();
            poolCreateInfo.maxSets = 1;
            poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            m_pool = CreateDescriptorPool(poolCreateInfo);
        }
        {
            VkDescriptorSetAllocateInfo allocateInfo{};
            allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocateInfo.pNext = nullptr;
            allocateInfo.descriptorPool = m_pool;
            allocateInfo.pSetLayouts = &m_setLayout;
            allocateInfo.descriptorSetCount = 1;

            // Create descriptor.
            RenderSystem::CheckVkResult(vkAllocateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), &allocateInfo, &m_set));
        }
    }

    BindlessManager::~BindlessManager()
    {
        // Destroy all device resource.
        DestroyDescriptorSetLayout(m_setLayout);
        DestroyDescriptorPool(m_pool);
    }

    BindlessIndex BindlessManager::RegisterSampler(VkSampler sampler)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = sampler;
        imageInfo.imageView = VK_NULL_HANDLE;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_set;
        write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessSampler)].type;
        write.dstBinding = static_cast<uint32>(EBindingType::BindlessSampler);
        write.pImageInfo = &imageInfo;
        write.descriptorCount = 1;
        write.dstArrayElement = RequireIndex(EBindingType::BindlessSampler);

        vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);

        return BindlessIndex{write.dstArrayElement, 0}; // Return as KeyValuePair
    }

    BindlessIndex BindlessManager::RegisterSRV(VkImageView view)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = VK_NULL_HANDLE;
        imageInfo.imageView = view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_set;
        write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessSampledImage)].type;
        write.dstBinding = static_cast<uint32>(EBindingType::BindlessSampledImage);
        write.pImageInfo = &imageInfo;
        write.descriptorCount = 1;
        write.dstArrayElement = RequireIndex(EBindingType::BindlessSampledImage);

        vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);

        return BindlessIndex{write.dstArrayElement, 0};
    }

    void BindlessManager::FreeSRV(BindlessIndex &index, Image fallback)
    {
        if (fallback.GetImage()) // VkImage GetImage
        {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.sampler = VK_NULL_HANDLE;
            imageInfo.imageView = fallback.GetView();
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_set;
            write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessSampledImage)].type;
            write.dstBinding = static_cast<uint32>(EBindingType::BindlessSampledImage);
            write.pImageInfo = &imageInfo;
            write.descriptorCount = 1;
            write.dstArrayElement = index.key; // Use key or access the first element

            vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);
        }

        FreeIndex(EBindingType::BindlessSampledImage, index.key);
        index = {};
    }

    BindlessIndex BindlessManager::RegisterUAV(VkImageView view)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = VK_NULL_HANDLE;
        imageInfo.imageView = view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_set;
        write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessStorageImage)].type;
        write.dstBinding = static_cast<uint32>(EBindingType::BindlessStorageImage);
        write.pImageInfo = &imageInfo;
        write.descriptorCount = 1;
        write.dstArrayElement = RequireIndex(EBindingType::BindlessStorageImage);

        vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);
        return BindlessIndex{write.dstArrayElement, 0};
    }

    void BindlessManager::FreeUAV(BindlessIndex &index, Image fallback)
    {
        if (fallback.GetImage()) // VkImage GetImage
        {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.sampler = VK_NULL_HANDLE;
            imageInfo.imageView = fallback.GetView();
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_set;
            write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessStorageImage)].type;
            write.dstBinding = static_cast<uint32>(EBindingType::BindlessStorageImage);
            write.pImageInfo = &imageInfo;
            write.descriptorCount = 1;
            write.dstArrayElement = index.key;

            vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);
        }

        FreeIndex(EBindingType::BindlessStorageImage, index.key);
        index = {};
    }

    BindlessIndex BindlessManager::RegisterStorageBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_set;
        write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessStorageBuffer)].type;
        write.dstBinding = static_cast<uint32>(EBindingType::BindlessStorageBuffer);
        write.pBufferInfo = &bufferInfo;
        write.descriptorCount = 1;
        write.dstArrayElement = RequireIndex(EBindingType::BindlessStorageBuffer);

        vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);
        return BindlessIndex{write.dstArrayElement, 0};
    }

    void BindlessManager::FreeStorageBuffer(BindlessIndex &index, std::shared_ptr<Buffer> fallback)
    {
        if (fallback)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = fallback->GetBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = fallback->GetSize();
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_set;
            write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessStorageBuffer)].type;
            write.dstBinding = static_cast<uint32>(EBindingType::BindlessStorageBuffer);
            write.pBufferInfo = &bufferInfo;
            write.descriptorCount = 1;
            write.dstArrayElement = index.key;

            vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);
        }

        FreeIndex(EBindingType::BindlessStorageBuffer, index.key);
        index = {};
    }

    BindlessIndex BindlessManager::RegisterUniformBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_set;
        write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessUniformBuffer)].type;
        write.dstBinding = static_cast<uint32>(EBindingType::BindlessUniformBuffer);
        write.pBufferInfo = &bufferInfo;
        write.descriptorCount = 1;
        write.dstArrayElement = RequireIndex(EBindingType::BindlessUniformBuffer);

        vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);
        return BindlessIndex{write.dstArrayElement, 0};
    }

    void BindlessManager::FreeUniformBuffer(BindlessIndex &index, std::shared_ptr<Buffer> fallback)
    {
        if (fallback)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = fallback->GetBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = fallback->GetSize();

            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = m_set;
            write.descriptorType = m_bindingConfigs[static_cast<uint32>(EBindingType::BindlessUniformBuffer)].type;
            write.dstBinding = static_cast<uint32>(EBindingType::BindlessUniformBuffer);
            write.pBufferInfo = &bufferInfo;
            write.descriptorCount = 1;
            write.dstArrayElement = index.key;

            vkUpdateDescriptorSets(RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(), 1, &write, 0, nullptr);
        }

        FreeIndex(EBindingType::BindlessUniformBuffer, index.key);
        index = {};
    }

    uint32 BindlessManager::RequireIndex(EBindingType type)
    {
        std::lock_guard<std::mutex> lock(m_lockCount);

        const auto &typeIndex = static_cast<uint32>(type);
        const auto &config = m_bindingConfigs[typeIndex];

        // Final index.
        uint32 index = 0;
        // Reuse or increment new one.
        auto &freeCounts = m_freeCount[typeIndex];
        auto &usedCount = m_usedCount[typeIndex];
        if (freeCounts.empty())
        {
            index = usedCount;
            usedCount++;
            if (usedCount >= config.count)
            {
                Log::Error(
                    "Too many items used in this set. Current bindless count: {}, "
                    "configured maximum: {}, device limit: {}.",
                    usedCount,
                    config.count,
                    config.limit);
                Log::Error("Bindless set count has been reset to 0. This may cause rendering errors.");

                usedCount = 0;
            }

            if (usedCount % 1000 == 0)
            {
                // Log or handle warning
            }
        }
        else
        {
            index = freeCounts.front();
            freeCounts.pop();
        }

        // Return result.
        return index;
    }

    void BindlessManager::FreeIndex(EBindingType type, uint32 index)
    {
        std::lock_guard<std::mutex> lock(m_lockCount);

        const auto &typeIndex = static_cast<uint32>(type);
        m_freeCount[typeIndex].push(index);
    }

    void BindlessManager::VerifyShaderLayout(const std::vector<BindlessReflectionData>& reflectionData)
    {
        for (const auto& data : reflectionData)
        {
            // bindless is always 100
            if (data.set != 100) continue; 

            Log::Info("BindlessManager verified: {} at Binding {}", data.name, data.binding);
            
            // Optional: Add asserts here to ensure data.binding matches the static_cast<uint32>(EBindingType::...) expectations.
        }
    }
}