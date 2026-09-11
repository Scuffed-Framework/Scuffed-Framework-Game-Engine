#pragma once

#include "Image.hpp"

namespace SF::Engine
{
    /**
     * @brief Resource that represents an array of 2D images.
     */
    class Image2dArray : public Image
    {
        SF_RTTI(Image2dArray, Image)
    public:
        /**
         * Creates a new array of 2D images.
         * @param extent The images extent in pixels.
         * @param arrayLayers The number of layers in the image.
         * @param format The format and type of the texel blocks that will be contained in the image.
         * @param layout The layout that the image subresources accessible from.
         * @param usage The intended usage of the image.
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param anisotropic If anisotropic filtering is enabled.
         * @param mipmap If mipmaps will be generated.
         */
        Image2dArray(const UVec2 &extent, uint32_t arrayLayers, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                     VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                     bool anisotropic = false, bool mipmap = false);

        /**
         * Creates a new array of 2D images from bitmap data.
         * @param bitmap The bitmap to load from (will be replicated to all layers).
         * @param arrayLayers The number of layers in the image.
         * @param format The format and type of the texel blocks that will be contained in the image.
         * @param layout The layout that the image subresources accessible from.
         * @param usage The intended usage of the image.
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param anisotropic If anisotropic filtering is enabled.
         * @param mipmap If mipmaps will be generated.
         */
        Image2dArray(std::unique_ptr<Bitmap> &&bitmap, uint32_t arrayLayers, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                     VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                     bool anisotropic = false, bool mipmap = false);

        /**
         * Sets the pixels of a specific layer of this image array.
         * @param pixels The pixels to copy from.
         * @param arrayLayer The layer to copy into.
         */
        void SetPixels(const uint8_t *pixels, uint32_t arrayLayer);

        bool IsAnisotropic() const { return anisotropic; }
        bool IsMipmap() const { return mipmap; }

        // Image2dArray
        void Serialize(XMLNode &node) const override
        {
            Image::Serialize(node);
            node.SetAttribute("anisotropic", anisotropic);
            node.SetAttribute("mipmap", mipmap);
        }

        void Deserialize(const XMLNode &node) override
        {
            Image::Deserialize(node);
            node.GetAttribute("anisotropic", anisotropic);
            node.GetAttribute("mipmap", mipmap);
        }

    private:
        bool anisotropic;
        bool mipmap;
    };
}