#pragma once
#include "../Buffer.hpp"

namespace SF::Engine::RHI
{
    class VulkanBuffer : public Buffer
    {
    public:
        VulkanBuffer(VkDeviceSize size, BufferUsageFlags usage, MemoryUsage memoryUsage = MemoryUsage::Auto,
                     MemoryAllocationFlags allocationFlags = 0, span<const std::byte> data = {}) :
            Buffer(size, usage, memoryUsage, allocationFlags, data)
        {
        }
        ~VulkanBuffer() override = default;

        void MapMemory(void **data) override;
        void UnmapMemory() override;
        void FlushMemory(DeviceSize offset, DeviceSize size) override;
        void InvalidateMemory(DeviceSize offset, DeviceSize size) override;
        BufferHandle *GetNativeBufferHandle() override;
    };
} // namespace SF::Engine::RHI
