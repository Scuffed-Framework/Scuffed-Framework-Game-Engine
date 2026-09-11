#pragma once

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <volk.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace SF::Engine
{
    class OffsetSize
    {
    public:
        OffsetSize(uint32_t offset, uint32_t size) : offset(offset), size(size) {}

        [[nodiscard]] uint32_t GetOffset() const noexcept
        {
            return offset;
        }

        [[nodiscard]] uint32_t GetSize() const noexcept
        {
            return size;
        }

        bool operator==(const OffsetSize& rhs) const = default;

    private:
        uint32_t offset;
        uint32_t size;
    };

    /**
     * @brief Class that holds image or buffer information for descriptor writes
     */
    class WriteDescriptorSetInformation
    {
    public:
        WriteDescriptorSetInformation(const VkWriteDescriptorSet& writeDescriptorSet,
                                      const VkDescriptorImageInfo& imageInfo)
            : writeDescriptorSet(writeDescriptorSet),
              imageInfo(std::make_unique<VkDescriptorImageInfo>(imageInfo))
        {
            this->writeDescriptorSet.pImageInfo = this->imageInfo.get();
        }

        WriteDescriptorSetInformation(const VkWriteDescriptorSet& writeDescriptorSet,
                                      const VkDescriptorBufferInfo& bufferInfo)
            : writeDescriptorSet(writeDescriptorSet),
              bufferInfo(std::make_unique<VkDescriptorBufferInfo>(bufferInfo))
        {
            this->writeDescriptorSet.pBufferInfo = this->bufferInfo.get();
        }

        [[nodiscard]] const VkWriteDescriptorSet& GetWriteDescriptorSet() const noexcept
        {
            return writeDescriptorSet;
        }

    private:
        VkWriteDescriptorSet writeDescriptorSet;
        std::unique_ptr<VkDescriptorImageInfo> imageInfo;
        std::unique_ptr<VkDescriptorBufferInfo> bufferInfo;
    };

    class Descriptor
    {
    public:
        Descriptor() = default;
        virtual ~Descriptor() = default;

        // Delete copy, allow move
        Descriptor(const Descriptor&) = delete;
        Descriptor& operator=(const Descriptor&) = delete;
        Descriptor(Descriptor&&) noexcept = default;
        Descriptor& operator=(Descriptor&&) noexcept = default;

        [[nodiscard]] virtual WriteDescriptorSetInformation GetWriteDescriptor(
            uint32_t binding, VkDescriptorType descriptorType,
            const std::optional<OffsetSize>& offsetSize) const = 0;
    };
}