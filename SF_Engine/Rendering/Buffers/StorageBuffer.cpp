#include "StorageBuffer.hpp"
#include <algorithm>
#include <ranges>
#include "Rendering/RenderSystem.hpp"

namespace SF::Engine
{
    StorageBuffer::StorageBuffer(VkDeviceSize size, std::span<const std::byte> data)
        : Buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                 VMA_MEMORY_USAGE_AUTO, // VmaMemoryUsage, not VkMemoryPropertyFlags
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT, // VmaAllocationCreateFlags
                 data)
    {
    }
    void StorageBuffer::Update(std::span<const std::byte> newData)
    {
        void *data;
        MapMemory(&data);
        std::ranges::copy(newData, static_cast<std::byte *>(data));
        UnmapMemory();
    }

    WriteDescriptorSetInformation StorageBuffer::GetWriteDescriptor(
        uint32_t binding, VkDescriptorType descriptorType,
        const std::optional<OffsetSize> &offsetSize) const
    {
        VkDescriptorBufferInfo bufferInfo = {.buffer = buffer_,
                                             .offset = offsetSize ? offsetSize->GetOffset() : 0,
                                             .range = offsetSize ? offsetSize->GetSize() : size_};

        VkWriteDescriptorSet descriptorWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = VK_NULL_HANDLE, // Will be set in the descriptor handler
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = descriptorType,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr, // Set later
            .pTexelBufferView = nullptr};

        return {descriptorWrite, bufferInfo};
    }

    VkDescriptorSetLayoutBinding StorageBuffer::GetDescriptorSetLayout(
        uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stage,
        [[maybe_unused]] uint32_t count)
    {
        return VkDescriptorSetLayoutBinding{.binding = binding,
                                            .descriptorType = descriptorType,
                                            .descriptorCount = 1,
                                            .stageFlags = stage,
                                            .pImmutableSamplers = nullptr};
    }
}