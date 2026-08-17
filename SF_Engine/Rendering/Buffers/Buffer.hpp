#pragma once

#define VK_NO_PROTOTYPES

#include <vk_mem_alloc.h>
#include <volk.h>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <concepts>
#include <memory>
#include <optional>
#include <span>

#ifdef Status
#undef Status
#endif

namespace SF::Engine
{
    // Modern concepts for type safety
    template <typename T>
    concept TriviallyCopiable = std::is_trivially_copyable_v<T>;

    template <typename T>
    concept StandardLayout = std::is_standard_layout_v<T>;

    /**
     * @brief Modern C++20 interface that represents a buffer with VMA allocation.
     */
    class Buffer
    {
    public:
        enum class Status
        {
            Reset,
            Changed,
            Normal,
            OverFlow
        };

        /**
         * Creates a new buffer with VMA and modern span interface.
         * @param size Size of the buffer in bytes.
         * @param usage Usage flag bitmask for the buffer.
         * @param memoryUsage VMA memory usage hint for optimal allocation.
         * @param allocationFlags Additional VMA allocation flags.
         * @param data Optional data to copy to the buffer after creation.
         */
        Buffer(VkDeviceSize size, VkBufferUsageFlags usage,
               VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO,
               VmaAllocationCreateFlags allocationFlags = 0, std::span<const std::byte> data = {});

        virtual ~Buffer();

        // Modern rule of five with explicit semantics
        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;
        Buffer(Buffer &&) noexcept;
        Buffer &operator=(Buffer &&) noexcept;

        void MapMemory(void **data);
        void UnmapMemory();
        void FlushMemory(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
        void InvalidateMemory(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

        // Modern getters with [[nodiscard]] attribute
        [[nodiscard]] constexpr VkDeviceSize GetSize() const noexcept
        {
            return size_;
        }
        [[nodiscard]] constexpr VkBuffer GetBuffer() const noexcept
        {
            return buffer_;
        }
        [[nodiscard]] constexpr VmaAllocation GetAllocation() const noexcept
        {
            return allocation_;
        }
        [[nodiscard]] VmaAllocationInfo GetAllocationInfo() const;
        [[nodiscard]] bool IsMapped() const noexcept
        {
            return mappedData_ != nullptr;
        }

        static void InsertMemoryBarrier(const CommandBuffer &commandBuffer, VkBuffer buffer,
                                        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                        VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask, VkDeviceSize offset = 0,
                                        VkDeviceSize size = VK_WHOLE_SIZE);

    protected:
        VkDeviceSize size_;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        void *mappedData_ = nullptr;
        bool persistentlyMapped_ = false;
    };
}