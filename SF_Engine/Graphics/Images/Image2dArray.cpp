#include "Image2dArray.hpp"

#include <Bitmaps/Bitmap.hpp>
#include <Graphics/Buffers/Buffer.hpp>
#include <Graphics/RenderSystem.hpp>

namespace SF::Engine
{
    Image2dArray::Image2dArray(const Vector2Uint &extent, uint32_t arrayLayers, VkFormat format, VkImageLayout layout, VkImageUsageFlags usage,
                               VkFilter filter, VkSamplerAddressMode addressMode, bool anisotropic, bool mipmap)
        : Image(filter, addressMode, VK_SAMPLE_COUNT_1_BIT, layout,
                usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                format, 1, arrayLayers, {extent.x, extent.y, 1}),
          anisotropic(anisotropic),
          mipmap(mipmap)
    {
        if (this->extent.width == 0 || this->extent.height == 0)
        {
            return;
        }

        mipLevels = mipmap ? GetMipLevels(this->extent) : 1;

        VmaAllocationInfo allocInfo;
        CreateImage(image, allocation, this->extent, format, samples, VK_IMAGE_TILING_OPTIMAL,
                    this->usage, VMA_MEMORY_USAGE_GPU_ONLY, mipLevels, 1, VK_IMAGE_TYPE_2D);
        CreateImageSampler(sampler, filter, addressMode, anisotropic, mipLevels);
        CreateImageView(image, view, VK_IMAGE_VIEW_TYPE_2D_ARRAY, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, arrayLayers, 0);
        TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_UNDEFINED, layout, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, arrayLayers, 0);
    }

    Image2dArray::Image2dArray(std::unique_ptr<Bitmap> &&bitmap, uint32_t arrayLayers, VkFormat format, VkImageLayout layout, VkImageUsageFlags usage,
                               VkFilter filter, VkSamplerAddressMode addressMode, bool anisotropic, bool mipmap)
        : Image(filter, addressMode, VK_SAMPLE_COUNT_1_BIT, layout,
                usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                format, 1, arrayLayers, {bitmap->GetSize().x, bitmap->GetSize().y, 1}),
          anisotropic(anisotropic),
          mipmap(mipmap)
    {
        if (extent.width == 0 || extent.height == 0)
        {
            return;
        }

        mipLevels = mipmap ? GetMipLevels(extent) : 1;

        VmaAllocationInfo allocInfo;
        CreateImage(image, allocation, this->extent, format, samples, VK_IMAGE_TILING_OPTIMAL,
                    this->usage, VMA_MEMORY_USAGE_GPU_ONLY, mipLevels, 1, VK_IMAGE_TYPE_2D);
        CreateImageSampler(sampler, filter, addressMode, anisotropic, mipLevels);
        CreateImageView(image, view, VK_IMAGE_VIEW_TYPE_2D_ARRAY, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, arrayLayers, 0);

        // Transition to transfer dst for uploading
        TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, arrayLayers, 0);

        // Create staging buffer - replicate the bitmap data for all array layers
        VkDeviceSize layerSize = bitmap->GetLength();
        VkDeviceSize totalSize = layerSize * arrayLayers;

        Buffer bufferStaging(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        uint8_t *data;
        bufferStaging.MapMemory(reinterpret_cast<void **>(&data));

        // Copy bitmap data to each layer
        for (uint32_t layer = 0; layer < arrayLayers; layer++)
        {
            std::memcpy(data + (layer * layerSize), bitmap->GetData().get(), layerSize);
        }
        bufferStaging.UnmapMemory();

        // Setup buffer copy regions for each array layer
        std::vector<VkBufferImageCopy> bufferCopyRegions;
        bufferCopyRegions.reserve(arrayLayers);

        for (uint32_t layer = 0; layer < arrayLayers; layer++)
        {
            VkBufferImageCopy region = {};
            region.bufferOffset = layer * layerSize;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = layer;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = extent;
            bufferCopyRegions.push_back(region);
        }

        // Copy buffer to image
        CommandBuffer commandBuffer;
        vkCmdCopyBufferToImage(commandBuffer, bufferStaging.GetBuffer(), image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<uint32_t>(bufferCopyRegions.size()),
                               bufferCopyRegions.data());
        commandBuffer.SubmitIdle();

        if (mipmap)
        {
            CreateMipmaps(image, extent, format, layout, mipLevels, 0, arrayLayers);
        }
        else
        {
            TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout,
                                  VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, arrayLayers, 0);
        }
    }

    void Image2dArray::SetPixels(const uint8_t *pixels, uint32_t arrayLayer)
    {
        // Calculate size based on format - assuming 4 bytes per pixel for R8G8B8A8
        uint32_t bytesPerPixel = 4;
        VkDeviceSize layerSize = extent.width * extent.height * bytesPerPixel;

        Buffer bufferStaging(layerSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        void *data;
        bufferStaging.MapMemory(&data);
        std::memcpy(data, pixels, layerSize);
        bufferStaging.UnmapMemory();

        CopyBufferToImage(bufferStaging.GetBuffer(), image, extent, 1, arrayLayer);
    }
}