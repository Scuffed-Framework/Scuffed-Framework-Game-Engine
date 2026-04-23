#include "Image2d.hpp"

#include <Bitmaps/Bitmap.hpp>
#include <Graphics/Buffers/Buffer.hpp>
#include <Graphics/RenderSystem.hpp>

namespace SF::Engine
{
    std::shared_ptr<Image2d> Image2d::Create(const std::filesystem::path &filename, VkFilter filter, VkSamplerAddressMode addressMode, bool anisotropic, bool mipmap)
    {
        // Resource system can cache this if needed
        return std::make_shared<Image2d>(filename, filter, addressMode, anisotropic, mipmap, true);
    }

    Image2d::Image2d(std::filesystem::path filename, VkFilter filter, VkSamplerAddressMode addressMode, bool anisotropic, bool mipmap, bool load)
        : Image(filter, addressMode, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_FORMAT_R8G8B8A8_UNORM, 1, 1, {0, 0, 1}),
          filename(std::move(filename)),
          anisotropic(anisotropic),
          mipmap(mipmap)
    {
        if (load)
        {
            Load();
        }
    }

    Image2d::Image2d(const Vector2Uint &extent, VkFormat format, VkImageLayout layout, VkImageUsageFlags usage, VkFilter filter, VkSamplerAddressMode addressMode,
                     VkSampleCountFlagBits samples, bool anisotropic, bool mipmap)
        : Image(filter, addressMode, samples, layout,
                usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                format, 1, 1, {extent.x, extent.y, 1}),
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
        CreateImageView(image, view, VK_IMAGE_VIEW_TYPE_2D, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, 1, 0);
        TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_UNDEFINED, layout, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, 1, 0);
    }

    Image2d::Image2d(std::unique_ptr<Bitmap> &&bitmap, VkFormat format, VkImageLayout layout, VkImageUsageFlags usage, VkFilter filter, VkSamplerAddressMode addressMode,
                     VkSampleCountFlagBits samples, bool anisotropic, bool mipmap)
        : Image(filter, addressMode, samples, layout,
                usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                format, 1, 1, {bitmap->GetSize().x, bitmap->GetSize().y, 1}),
          anisotropic(anisotropic),
          mipmap(mipmap)
    {
        if (extent.width == 0 || extent.height == 0)
        {
            return;
        }

        Load(std::move(bitmap));
    }

    void Image2d::Load(std::unique_ptr<Bitmap> loadBitmap)
    {
        if (!loadBitmap)
        {
            loadBitmap = std::make_unique<Bitmap>(filename);
        }

        extent = {loadBitmap->GetSize().x, loadBitmap->GetSize().y, 1};
        components = loadBitmap->GetBytesPerPixel();
        mipLevels = mipmap ? GetMipLevels(extent) : 1;

        VmaAllocationInfo allocInfo;
        CreateImage(image, allocation, this->extent, format, samples, VK_IMAGE_TILING_OPTIMAL,
                    this->usage, VMA_MEMORY_USAGE_GPU_ONLY, mipLevels, 1, VK_IMAGE_TYPE_2D);
        CreateImageSampler(sampler, filter, addressMode, anisotropic, mipLevels);
        CreateImageView(image, view, VK_IMAGE_VIEW_TYPE_2D, format, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, 1, 0);

        // Transition to transfer dst for uploading
        TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, 1, 0);

        // Create staging buffer and upload data
        Buffer bufferStaging(loadBitmap->GetLength(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VMA_MEMORY_USAGE_CPU_ONLY);

        uint8_t *data;
        bufferStaging.MapMemory(reinterpret_cast<void **>(&data));
        std::memcpy(data, loadBitmap->GetData().get(), bufferStaging.GetSize());
        bufferStaging.UnmapMemory();

        CopyBufferToImage(bufferStaging.GetBuffer(), image, extent, 1, 0);

        if (mipmap)
        {
            CreateMipmaps(image, extent, format, layout, mipLevels, 0, 1);
        }
        else
        {
            TransitionImageLayout(image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout,
                                  VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, 0, 1, 0);
        }
    }

    void Image2d::SetPixels(const uint8_t *pixels, uint32_t layerCount, uint32_t baseArrayLayer)
    {
        Buffer bufferStaging(extent.width * extent.height * components * layerCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VMA_MEMORY_USAGE_CPU_ONLY);

        void *data;
        bufferStaging.MapMemory(&data);
        std::memcpy(data, pixels, bufferStaging.GetSize());
        bufferStaging.UnmapMemory();

        CopyBufferToImage(bufferStaging.GetBuffer(), image, extent, layerCount, baseArrayLayer);
    }
}