#include "Cubemap.hpp"

#include <cstring>

#include <Bitmaps/Bitmap.hpp>
#include <Graphics/RenderSystem.hpp>
#include "Image.hpp"

namespace SF::Engine
{
    Cubemap::Cubemap(const std::filesystem::path& filename, const std::string& fileSuffix,
                     VkFilter filter, VkSamplerAddressMode addressMode, bool anisotropic,
                     bool mipmap)
        : Image(filter, addressMode, VK_SAMPLE_COUNT_1_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_FORMAT_R8G8B8A8_UNORM, 1, 6, {0, 0, 1}),
          filename(filename),
          fileSuffix(fileSuffix),
          anisotropic(anisotropic),
          mipmap(mipmap)
    {
        Load();
    }

    Cubemap::Cubemap(const Vector2Uint& extent, VkFormat format, VkImageLayout layout,
                     VkImageUsageFlags usage, VkFilter filter, VkSamplerAddressMode addressMode,
                     VkSampleCountFlagBits samples, bool anisotropic, bool mipmap)
        : Image(filter, addressMode, samples, layout,
                usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                format, 1, 6, {extent.x, extent.y, 1}),
          anisotropic(anisotropic),
          mipmap(mipmap),
          components(4)
    {
        Load();
    }

    Cubemap::Cubemap(std::unique_ptr<Bitmap>&& bitmap, VkFormat format, VkImageLayout layout,
                     VkImageUsageFlags usage, VkFilter filter, VkSamplerAddressMode addressMode,
                     VkSampleCountFlagBits samples, bool anisotropic, bool mipmap)
        : Image(filter, addressMode, samples, layout,
                usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT,
                format, 1, 6, {bitmap->GetSize().x, bitmap->GetSize().y, 1}),
          anisotropic(anisotropic),
          mipmap(mipmap),
          components(bitmap->GetBytesPerPixel())
    {
        Load(std::move(bitmap));
    }

    std::unique_ptr<Bitmap> Cubemap::GetBitmap(uint32_t mipLevel) const
    {
        auto size = Vector2Uint(extent.width, extent.height) >> mipLevel;
        auto sizeSide = size.x * size.y * components;
        auto bitmap =
            std::make_unique<Bitmap>(Vector2Uint(size.x, size.y * arrayLayers), components);
        auto offset = bitmap->GetData().get();

        for (uint32_t i = 0; i < 6; i++)
        {
            auto bitmapSide = Image::GetBitmap(mipLevel, i);
            std::memcpy(offset, bitmapSide->GetData().get(), sizeSide);
            offset += sizeSide;
        }

        return bitmap;
    }

    void Cubemap::SetPixels(const uint8_t* pixels, uint32_t layerCount, uint32_t baseArrayLayer)
    {
        VkDeviceSize bufferSize = extent.width * extent.height * components * arrayLayers;

        // Create staging buffer with VMA
        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = bufferSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VkBuffer stagingBuffer;
        VmaAllocation stagingAllocation;
        vmaCreateBuffer(*RenderSystem::Get()->getAllocator(), &bufferInfo, &allocInfo,
                        &stagingBuffer, &stagingAllocation, nullptr);

        // Map and copy data
        void* data;
        vmaMapMemory(*RenderSystem::Get()->getAllocator(), stagingAllocation, &data);
        std::memcpy(data, pixels, bufferSize);
        vmaUnmapMemory(*RenderSystem::Get()->getAllocator(), stagingAllocation);

        // Copy to image
        CopyBufferToImage(stagingBuffer, image, extent, layerCount, baseArrayLayer);

        // Cleanup
        vmaDestroyBuffer(*RenderSystem::Get()->getAllocator(), stagingBuffer, stagingAllocation);
    }

    void Cubemap::Load(std::unique_ptr<Bitmap> loadBitmap)
    {
        if (!filename.empty() && !loadBitmap)
        {
            uint8_t* offset = nullptr;

            for (const auto& side : fileSides)
            {
                Bitmap bitmapSide(filename / (side + fileSuffix));
                auto lengthSide = bitmapSide.GetLength();

                if (!loadBitmap)
                {
                    loadBitmap = std::make_unique<Bitmap>(
                        std::make_unique<uint8_t[]>(lengthSide * arrayLayers), bitmapSide.GetSize(),
                        bitmapSide.GetBytesPerPixel());
                    offset = loadBitmap->GetData().get();
                }

                std::memcpy(offset, bitmapSide.GetData().get(), lengthSide);
                offset += lengthSide;
            }

            extent = {loadBitmap->GetSize().y, loadBitmap->GetSize().y, 1};
            components = loadBitmap->GetBytesPerPixel();
        }

        if (extent.width == 0 || extent.height == 0)
        {
            return;
        }

        mipLevels = mipmap ? GetMipLevels(extent) : 1;

        CreateImage(image, allocation, extent, format, samples, VK_IMAGE_TILING_OPTIMAL, usage,
                    VMA_MEMORY_USAGE_GPU_ONLY, mipLevels, arrayLayers, VK_IMAGE_TYPE_2D);
        CreateImageSampler(sampler, filter, addressMode, anisotropic, mipLevels);
        CreateImageView(image, view, VK_IMAGE_VIEW_TYPE_CUBE, format, VK_IMAGE_ASPECT_COLOR_BIT,
                        mipLevels, 0, arrayLayers, 0);

        if (loadBitmap || mipmap)
        {
            TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT,
                                  mipLevels, 0, arrayLayers, 0);
        }

        if (loadBitmap)
        {
            VkDeviceSize bufferSize = loadBitmap->GetLength() * arrayLayers;

            // Create staging buffer with VMA
            VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

            VkBuffer stagingBuffer;
            VmaAllocation stagingAllocation;
            vmaCreateBuffer(*RenderSystem::Get()->getAllocator(), &bufferInfo, &allocInfo,
                            &stagingBuffer, &stagingAllocation, nullptr);

            // Map and copy data
            void* data;
            vmaMapMemory(*RenderSystem::Get()->getAllocator(), stagingAllocation, &data);
            std::memcpy(data, loadBitmap->GetData().get(), bufferSize);
            vmaUnmapMemory(*RenderSystem::Get()->getAllocator(), stagingAllocation);

            // Copy to image
            CopyBufferToImage(stagingBuffer, image, extent, arrayLayers, 0);

            // Cleanup
            vmaDestroyBuffer(*RenderSystem::Get()->getAllocator(), stagingBuffer,
                             stagingAllocation);
        }

        if (mipmap)
        {
            CreateMipmaps(image, extent, format, layout, mipLevels, 0, arrayLayers);
        }
        else if (loadBitmap)
        {
            TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout,
                                  VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, arrayLayers, 0);
        }
        else
        {
            TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_UNDEFINED, layout,
                                  VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, arrayLayers, 0);
        }
    }
}