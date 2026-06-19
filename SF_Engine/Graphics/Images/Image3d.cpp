#include "Image3d.hpp"
#include <Graphics/Buffers/Buffer.hpp>
#include <Graphics/RenderSystem.hpp>
#include <Math/Vectors/Vector3.hpp>

namespace SF::Engine
{

    Image3d::Image3d(const VkExtent3D &extent,
                     VkFormat format,
                     VkImageLayout layout,
                     VkImageUsageFlags usage,
                     VkFilter filter,
                     VkSamplerAddressMode addressMode,
                     VkSampleCountFlagBits samples,
                     bool anisotropic,
                     bool mipmap)
        : Image(filter, addressMode, samples, layout,
                usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                format,
                mipmap ? GetMipLevels(extent) : 1,
                1,
                extent),
          anisotropic_(anisotropic),
          mipmap_(mipmap)
    {
        InternalCreate();
    }

    Image3d::Image3d(const VkExtent3D &extent,
                     const uint8_t *voxels,
                     size_t voxelSizeBytes,
                     VkFormat format,
                     VkImageLayout layout,
                     VkImageUsageFlags usage,
                     VkFilter filter,
                     VkSamplerAddressMode addressMode,
                     VkSampleCountFlagBits samples,
                     bool anisotropic,
                     bool mipmap)
        : Image(filter, addressMode, samples, layout,
                usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                format,
                mipmap ? GetMipLevels(extent) : 1,
                1,
                extent),
          anisotropic_(anisotropic),
          mipmap_(mipmap),
          voxelSize_(voxelSizeBytes)
    {
        InternalCreate();
        Upload(voxels, voxelSizeBytes);
    }

    void Image3d::InternalCreate()
    {
        CreateImage(
            image,
            allocation,
            extent,
            format,
            samples,
            VK_IMAGE_TILING_OPTIMAL,
            usage,
            VMA_MEMORY_USAGE_GPU_ONLY,
            mipLevels,
            1,
            VK_IMAGE_TYPE_3D);

        CreateImageSampler(sampler, filter, addressMode, anisotropic_, mipLevels);

        CreateImageView(
            image,
            view,
            VK_IMAGE_VIEW_TYPE_3D,
            format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            mipLevels,
            0,
            1,
            0);

        TransitionImageLayout(
            image,
            format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            layout,
            VK_IMAGE_ASPECT_COLOR_BIT,
            mipLevels,
            0,
            1,
            0);
    }

    void Image3d::Upload(const uint8_t *voxels, size_t voxelSize)
    {
        const size_t totalBytes =
            static_cast<size_t>(extent.width) *
            extent.height *
            extent.depth *
            voxelSize;

        voxelSize_ = voxelSize;

        Buffer staging(totalBytes,
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VMA_MEMORY_USAGE_CPU_ONLY);

        void *map;
        staging.MapMemory(&map);
        std::memcpy(map, voxels, totalBytes);
        staging.UnmapMemory();

        CopyBufferToImage(staging.GetBuffer(), image, extent, 1, 0);

        if (mipmap_ && mipLevels > 1)
            CreateMipmaps(image, extent, format, layout, mipLevels, 0, 1);
    }

    void Image3d::SetPixels3D(const uint8_t *voxels, size_t voxelSizeBytes)
    {
        Upload(voxels, voxelSizeBytes);
    }

}