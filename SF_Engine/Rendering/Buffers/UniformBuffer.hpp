#pragma once

#include <Rendering/Descriptors/BasicDescriptor.hpp>
#include <span>
#include "Buffer.hpp"

namespace SF::Engine
{
    class UniformBuffer : public Descriptor, public Buffer
    {
    public:
        explicit UniformBuffer(VkDeviceSize size, std::span<const std::byte> data = {});

        template <TriviallyCopiable T>
        void Update(const T& object)
        {
            auto byteSpan = std::as_bytes(std::span{&object, 1});
            Update(byteSpan);
        }

        template <TriviallyCopiable T>
        void Update(std::span<const T> newData)
        {
            void* data;
            MapMemory(&data);

            auto byteSpan = std::as_bytes(newData);
            std::ranges::copy(byteSpan, static_cast<std::byte*>(data));

            UnmapMemory();
        }

        void Update(std::span<const std::byte> newData);

        [[nodiscard]] WriteDescriptorSetInformation GetWriteDescriptor(
            uint32_t binding, VkDescriptorType descriptorType,
            const std::optional<OffsetSize>& offsetSize) const override;

        [[nodiscard]] static VkDescriptorSetLayoutBinding GetDescriptorSetLayout(
            uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stage,
            uint32_t count);
    };
}