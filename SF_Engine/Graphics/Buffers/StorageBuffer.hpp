#pragma once

#include "Buffer.hpp"
#include <Graphics/Descriptors/BasicDescriptor.hpp>

namespace SF::Engine
{
	class StorageBuffer : public Descriptor, public Buffer
	{
	public:
		explicit StorageBuffer(VkDeviceSize size, std::span<const std::byte> data = {});

		template <TriviallyCopiable T>
		void Update(std::span<const T> newData)
		{
			void *data;
			MapMemory(&data);

			auto byteSpan = std::as_bytes(newData);
			std::ranges::copy(byteSpan, static_cast<std::byte *>(data));

			UnmapMemory();
		}

		void Update(std::span<const std::byte> newData);

		[[nodiscard]] WriteDescriptorSetInformation GetWriteDescriptor(
			uint32_t binding,
			VkDescriptorType descriptorType,
			const std::optional<OffsetSize> &offsetSize) const override;

		[[nodiscard]] static VkDescriptorSetLayoutBinding GetDescriptorSetLayout(
			uint32_t binding,
			VkDescriptorType descriptorType,
			VkShaderStageFlags stage,
			uint32_t count);
	};
}
