#include "Buffer.hpp"

#include <Rendering/RenderSystem.hpp>
#include <algorithm>
#include <ranges>
#include <stdexcept>

namespace SF::Engine
{
    Buffer::Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage,
                   VmaAllocationCreateFlags allocationFlags, std::span<const std::byte> data)
        : size_(size)
    {
        auto logicalDevice = RenderSystem::Get()->GetLogicalDevice();
        auto *vmaAllocator = RenderSystem::Get()->GetAllocator();

        auto graphicsFamily = logicalDevice->GetGraphicsFamily();
        auto presentFamily = logicalDevice->GetPresentFamily();
        auto computeFamily = logicalDevice->GetComputeFamily();

        std::array queueFamily = {graphicsFamily, presentFamily, computeFamily};

        // Create the buffer handle with designated initializers
        VkBufferCreateInfo bufferCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = static_cast<uint32_t>(queueFamily.size()),
            .pQueueFamilyIndices = queueFamily.data()};

        // Configure VMA allocation
        VmaAllocationCreateInfo allocInfo = {.flags = allocationFlags,
                                             .usage = memoryUsage,
                                             .requiredFlags = 0,
                                             .preferredFlags = 0,
                                             .memoryTypeBits = 0,
                                             .pool = VK_NULL_HANDLE,
                                             .pUserData = nullptr,
                                             .priority = 0.5f};

        // For host-visible buffers, prefer mapped access
        if (memoryUsage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST ||
            (allocationFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT) ||
            (allocationFlags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT))
        {
            allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
            persistentlyMapped_ = true;
        }

        // Create buffer with VMA
        VmaAllocationInfo allocationInfo;
        RenderSystem::CheckVkResult(vmaCreateBuffer(*vmaAllocator, &bufferCreateInfo, &allocInfo,
                                                    &buffer_, &allocation_, &allocationInfo));

        // Store mapped pointer if buffer was created with MAPPED flag
        if (persistentlyMapped_)
        {
            mappedData_ = allocationInfo.pMappedData;
        }

        // If data has been provided, copy it to the buffer
        if (!data.empty()) [[likely]]
        {
            void *mapped = mappedData_;

            if (!mapped)
            {
                MapMemory(&mapped);
            }

            std::ranges::copy(data, static_cast<std::byte *>(mapped));

            // Flush if not coherent
            if (!(allocationInfo.memoryType & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                FlushMemory();
            }

            if (!persistentlyMapped_)
            {
                UnmapMemory();
            }
        }
    }

    Buffer::~Buffer()
    {
        if (buffer_ != VK_NULL_HANDLE)
        {
            auto *vmaAllocator = RenderSystem::Get()->GetAllocator();

            if (mappedData_ && !persistentlyMapped_)
            {
                UnmapMemory();
            }

            vmaDestroyBuffer(*vmaAllocator, buffer_, allocation_);
            buffer_ = VK_NULL_HANDLE;
            allocation_ = VK_NULL_HANDLE;
        }
    }

    Buffer::Buffer(Buffer &&other) noexcept
        : size_(other.size_),
          buffer_(other.buffer_),
          allocation_(other.allocation_),
          mappedData_(other.mappedData_),
          persistentlyMapped_(other.persistentlyMapped_)
    {
        other.buffer_ = VK_NULL_HANDLE;
        other.allocation_ = VK_NULL_HANDLE;
        other.mappedData_ = nullptr;
        other.size_ = 0;
    }

    Buffer &Buffer::operator=(Buffer &&other) noexcept
    {
        if (this != &other)
        {
            // Clean up existing resources
            if (buffer_ != VK_NULL_HANDLE)
            {
                auto *vmaAllocator = RenderSystem::Get()->GetAllocator();
                if (mappedData_ && !persistentlyMapped_)
                {
                    UnmapMemory();
                }
                vmaDestroyBuffer(*vmaAllocator, buffer_, allocation_);
            }

            // Move resources
            size_ = other.size_;
            buffer_ = other.buffer_;
            allocation_ = other.allocation_;
            mappedData_ = other.mappedData_;
            persistentlyMapped_ = other.persistentlyMapped_;

            // Reset source
            other.buffer_ = VK_NULL_HANDLE;
            other.allocation_ = VK_NULL_HANDLE;
            other.mappedData_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    void Buffer::MapMemory(void **data)
    {
        if (mappedData_)
        {
            *data = mappedData_;
            return;
        }

        auto *vmaAllocator = RenderSystem::Get()->GetAllocator();
        RenderSystem::CheckVkResult(vmaMapMemory(*vmaAllocator, allocation_, data));

        if (!persistentlyMapped_)
        {
            mappedData_ = *data;
        }
    }

    void Buffer::UnmapMemory()
    {
        if (persistentlyMapped_)
        {
            return; // Don't unmap persistently mapped buffers
        }

        if (mappedData_)
        {
            auto *vmaAllocator = RenderSystem::Get()->GetAllocator();
            vmaUnmapMemory(*vmaAllocator, allocation_);
            mappedData_ = nullptr;
        }
    }

    void Buffer::FlushMemory(VkDeviceSize offset, VkDeviceSize size)
    {
        auto *vmaAllocator = RenderSystem::Get()->GetAllocator();
        RenderSystem::CheckVkResult(vmaFlushAllocation(*vmaAllocator, allocation_, offset, size));
    }

    void Buffer::InvalidateMemory(VkDeviceSize offset, VkDeviceSize size)
    {
        auto *vmaAllocator = RenderSystem::Get()->GetAllocator();
        RenderSystem::CheckVkResult(
            vmaInvalidateAllocation(*vmaAllocator, allocation_, offset, size));
    }

    VmaAllocationInfo Buffer::GetAllocationInfo() const
    {
        VmaAllocationInfo info;
        auto *vmaAllocator = RenderSystem::Get()->GetAllocator();
        vmaGetAllocationInfo(*vmaAllocator, allocation_, &info);
        return info;
    }

    void Buffer::InsertMemoryBarrier(const CommandBuffer &commandBuffer, VkBuffer buffer,
                                     VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                     VkPipelineStageFlags srcStageMask,
                                     VkPipelineStageFlags dstStageMask, VkDeviceSize offset,
                                     VkDeviceSize size)
    {
        VkBufferMemoryBarrier bufferMemoryBarrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = srcAccessMask,
            .dstAccessMask = dstAccessMask,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buffer,
            .offset = offset,
            .size = size};

        vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 1,
                             &bufferMemoryBarrier, 0, nullptr);
    }
}