#pragma once

#include <Assets/Bitmaps/Bitmap.hpp>
#include "Image.hpp"
#include <typeindex>

namespace SF::Engine
{
    /**
     * @brief Resource that represents a 2D image.
     */
    class Image2d : public Image
    {
        SF_RTTI(Image2d, Image)
    public:
        /**
         * Creates a new 2D image, or finds one with the same values.
         * @param filename The file to load the image from.
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param anisotropic If anisotropic filtering is enabled.
         * @param mipmap If mipmaps will be generated.
         * @return The 2D image with the requested values.
         */
        static std::shared_ptr<Image2d> Create(const std::filesystem::path &filename, VkFilter filter = VK_FILTER_LINEAR,
                                               VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, bool anisotropic = true, bool mipmap = true);

        /**
         * Creates a new 2D image from file.
         * @param filename The file to load the image from.
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param anisotropic If anisotropic filtering is enabled.
         * @param mipmap If mipmaps will be generated.
         * @param load If this resource will be loaded immediately, otherwise {@link Image2d#Load} can be called later.
         */
        explicit Image2d(std::filesystem::path filename, VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                         bool anisotropic = true, bool mipmap = true, bool load = true);

        /**
         * Creates a new 2D image with specified dimensions.
         * @param extent The images extent in pixels.
         * @param format The format and type of the texel blocks that will be contained in the image.
         * @param layout The layout that the image subresources accessible from.
         * @param usage The intended usage of the image.
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param samples The number of samples per texel.
         * @param anisotropic If anisotropic filtering is enabled.
         * @param mipmap If mipmaps will be generated.
         */
        explicit Image2d(const UVec2 &extent, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                         VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false, bool mipmap = false);

        /**
         * Creates a new 2D image from bitmap data.
         * @param bitmap The bitmap to load from.
         * @param format The format and type of the texel blocks that will be contained in the image.
         * @param layout The layout that the image subresources accessible from.
         * @param usage The intended usage of the image.
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param samples The number of samples per texel.
         * @param anisotropic If anisotropic filtering is enabled.
         * @param mipmap If mipmaps will be generated.
         */
        explicit Image2d(std::unique_ptr<Bitmap> &&bitmap, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                         VkFilter filter = VK_FILTER_LINEAR, VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false, bool mipmap = false);

        /**
         * Sets the pixels of this image.
         * @param pixels The pixels to copy from.
         * @param layerCount The amount of layers contained in the pixels.
         * @param baseArrayLayer The first layer to copy into.
         */
        void SetPixels(const uint8_t *pixels, uint32_t layerCount, uint32_t baseArrayLayer);

        std::type_index GetTypeIndex() const { return typeid(Image2d); }

        const std::filesystem::path &GetFilename() const { return filename; }
        bool IsAnisotropic() const { return anisotropic; }
        bool IsMipmap() const { return mipmap; }
        uint32_t GetComponents() const { return components; }

        void Serialize(XMLNode &node) const
        {
            Image::Serialize(node);
            node.SetAttribute("filename", filename.string());
            node.SetAttribute("anisotropic", anisotropic);
            node.SetAttribute("mipmap", mipmap);
        }

        void Deserialize(const XMLNode &node)
        {
            Image::Deserialize(node);
            std::string f;
            node.GetAttribute("filename", f);
            filename = f;
            node.GetAttribute("anisotropic", anisotropic);
            node.GetAttribute("mipmap", mipmap);
        }

    private:
        void Load(std::unique_ptr<Bitmap> loadBitmap = nullptr);

        // Bytes-per-texel for formats constructible via the extent-based
        // ctor : that ctor never goes through Load() (which is the only
        // other place `components` gets set, from the source Bitmap), so
        // SetPixels() would otherwise stage a zero-size buffer for any
        // image built that way. Extend as new formats need SetPixels support.
        static uint32_t BytesPerPixelForFormat(VkFormat format);

        std::filesystem::path filename;
        bool anisotropic;
        bool mipmap;
        uint32_t components = 0;
    };
}