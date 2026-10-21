#pragma once

#include "Buffer.hpp"

namespace SF::Engine
{
    class InstanceBuffer : public Buffer
    {
    public:
        explicit InstanceBuffer(VkDeviceSize size);

        template<TriviallyCopiable T>
        void Update(std::span<const T> newData)
        {
            void *data;
            MapMemory(&data);

            auto byteSpan = std::as_bytes(newData);
            std::ranges::copy(byteSpan, static_cast<std::byte *>(data));

            UnmapMemory();
        }

        void Update(std::span<const std::byte> newData);
    };
} // namespace SF::Engine
