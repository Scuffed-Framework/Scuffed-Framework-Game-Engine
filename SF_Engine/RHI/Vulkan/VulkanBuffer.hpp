#pragma once
#include "../Buffer.hpp"

namespace SF::Engine::RHI
{
    class VulkanBuffer : public Buffer
    {
    public:
        VulkanBuffer() : Buffer(VkDeviceSize size, ) {}
        ~VulkanBuffer() override = default;

        void MapMemory(void **data) override;
        void UnmapMemory() override;
        void FlushMemory(DeviceSize offset = 0, DeviceSize size = WholeSize) override;
        void InvalidateMemory(DeviceSize offset = 0, DeviceSize size = WholeSize) override;
        BufferHandle *GetNativeBufferHandle() override;
    };
} // namespace SF::Engine::RHI
