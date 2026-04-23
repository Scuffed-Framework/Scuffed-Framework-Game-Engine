#include "InstanceBuffer.hpp"
#include <Graphics/RenderSystem.hpp>
#include <algorithm>
#include <ranges>

namespace SF::Engine
{
    InstanceBuffer::InstanceBuffer(VkDeviceSize size)
        : Buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VMA_MEMORY_USAGE_AUTO,  // or VMA_MEMORY_USAGE_CPU_TO_GPU
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                     VMA_ALLOCATION_CREATE_MAPPED_BIT)
    {
    }

    void InstanceBuffer::Update(const CommandBuffer& commandBuffer,
                                std::span<const std::byte> newData)
    {
        void* data;
        MapMemory(&data);

        std::ranges::copy(newData, static_cast<std::byte*>(data));

        UnmapMemory();  // Shocker
    }
}