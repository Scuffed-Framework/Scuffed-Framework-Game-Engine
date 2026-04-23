// InstanceBuffer.hpp
#pragma once

#include "Buffer.hpp"

namespace SF::Engine
{
	class InstanceBuffer : public Buffer
	{
	public:
		explicit InstanceBuffer(VkDeviceSize size);

		template <TriviallyCopiable T>
		void Update(const CommandBuffer &commandBuffer, std::span<const T> newData)
		{
			void *data;
			MapMemory(&data);

			auto byteSpan = std::as_bytes(newData);
			std::ranges::copy(byteSpan, static_cast<std::byte *>(data));

			UnmapMemory();
		}

		void Update(const CommandBuffer &commandBuffer, std::span<const std::byte> newData);
	};
}
