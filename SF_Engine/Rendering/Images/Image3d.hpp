#pragma once

#include "Image.hpp"
#include <Assets/Bitmaps/Bitmap.hpp>
#include <filesystem>
#include <memory>

namespace SF::Engine
{
    /**
     * @brief Resource that represents a 3D volume texture.
     */
    class Image3d : public Image
    {
        SF_RTTI(Image3d, Image)
    public:
        /**
         * Creates an empty 3D image with the given dimensions.
         */
        Image3d(const UVec3 &extent,
                VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VkFilter filter = VK_FILTER_LINEAR,
                VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                bool anisotropic = false,
                bool mipmap = false);

        /**
         * Creates a 3D image from in-memory voxel data.
         */
        Image3d(const UVec3 &extent,
                const uint8_t *voxels,
                size_t voxelSizeBytes,
                VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VkFilter filter = VK_FILTER_LINEAR,
                VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                bool anisotropic = false,
                bool mipmap = false);

        /**
         * Upload new voxel data into the volume texture.
         */
        void SetPixels3D(const uint8_t *voxels, size_t voxelSizeBytes);

        std::type_index GetTypeIndex() const { return typeid(Image3d); }

        bool IsAnisotropic() const { return anisotropic_; }
        bool IsMipmap() const { return mipmap_; }

        void Serialize(XMLNode &node) const override
        {
            Image::Serialize(node);
            node.SetAttribute("anisotropic", anisotropic_);
            node.SetAttribute("mipmap", mipmap_);
            node.SetAttribute("voxelSize", (int)voxelSize_);
        }

        void Deserialize(const XMLNode &node) override
        {
            Image::Deserialize(node);
            node.GetAttribute("anisotropic", anisotropic_);
            node.GetAttribute("mipmap", mipmap_);
            int vs;
            node.GetAttribute("voxelSize", vs);
            voxelSize_ = (size_t)vs;
        }

    private:
        void InternalCreate();
        void Upload(const uint8_t *voxels, size_t voxelSize);

        bool anisotropic_;
        bool mipmap_;
        size_t voxelSize_ = 0;
    };
}