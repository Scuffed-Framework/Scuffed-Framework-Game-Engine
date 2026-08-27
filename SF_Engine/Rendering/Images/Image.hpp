#pragma once

#include <vector>

#include <Rendering/Commands/CommandBuffer.hpp>
#include <Rendering/Descriptors/BasicDescriptor.hpp>
#include <Math/Vectors/Vector.hpp>
#include <vk_mem_alloc.h>
#include <filesystem>
#include <typeindex>
#include <type_traits>
#include <LowLevel/XML/XMLModule.hpp>

#include <Reflection/RTTISingle.hpp>
#include <Assets/SerializationRegistry.hpp>
#include <LowLevel/XML/XMLNodeWriter.hpp>

SF_REFLECT_EXTERNAL_TYPE(VkExtent3D)
SF_REFLECT_EXTERNAL_TYPE(VkExtent2D)

namespace SF::Engine
{
    class Bitmap;

    /**
     * @brief A representation of a Vulkan image, sampler, and view.
     */
    class Image : public Descriptor, public Serializable
    {
        SF_RTTI(Image, Serializable)
    public:
        /**
         * Creates a new image object.
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param samples The number of samples per texel.
         * @param layout The layout that the image subresources accessible from.
         * @param usage The intended usage of the image.
         * @param format The format and type of the texel blocks that will be contained in the image.
         * @param mipLevels The number of levels of detail available for minified sampling of the image.
         * @param arrayLayers The number of layers in the image.
         * @param extent The number of data elements in each dimension of the base level.
         */
        Image(VkFilter filter, VkSamplerAddressMode addressMode, VkSampleCountFlagBits samples, VkImageLayout layout, VkImageUsageFlags usage,
              VkFormat format, uint32_t mipLevels, uint32_t arrayLayers, const UVec3 &extent);

        ~Image();

        WriteDescriptorSetInformation GetWriteDescriptor(uint32_t binding, VkDescriptorType descriptorType, const std::optional<OffsetSize> &offsetSize) const override;
        static VkDescriptorSetLayoutBinding GetDescriptorSetLayout(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stage, uint32_t count);

        /**
         * Copies the images pixels from memory to a bitmap. If this method is called from multiple threads at the same time Vulkan will crash!
         * @param mipLevel The mipmap level index to sample.
         * @param arrayLayer The array level to sample.
         * @return A copy of the images pixels.
         */
        std::unique_ptr<Bitmap> GetBitmap(uint32_t mipLevel = 0, uint32_t arrayLayer = 0) const;

        const UVec3 &GetExtent() const { return extent; }
        UVec2 GetSize() const { return {extent.x, extent.y}; }
        VkFormat GetFormat() const { return format; }
        VkSampleCountFlagBits GetSamples() const { return samples; }
        VkImageUsageFlags GetUsage() const { return usage; }
        uint32_t GetMipLevels() const { return mipLevels; }
        uint32_t GetArrayLevels() const { return arrayLayers; }
        VkFilter GetFilter() const { return filter; }
        VkSamplerAddressMode GetAddressMode() const { return addressMode; }
        VkImageLayout GetLayout() const { return layout; }
        const VkImage &GetImage() { return image; }
        const VmaAllocation &GetAllocation() { return allocation; }
        const VkSampler &GetSampler() const { return sampler; }
        const VkImageView &GetView() const { return view; }

        static uint32_t GetMipLevels(const UVec3 &extent);

        void SetLayout(VkImageLayout newLayout) { layout = newLayout; }

        /**
         * Find a format in the candidates list that fits the tiling and features required.
         * @param candidates Formats that are tested for features, in order of preference.
         * @param tiling Tiling mode to test features in.
         * @param features The features to test for.
         * @return The format found, or VK_FORMAT_UNDEFINED.
         */
        static VkFormat FindSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        /**
         * Gets if a format has a depth component.
         * @param format The format to check.
         * @return If the format has a depth component.
         */
        static bool HasDepth(VkFormat format);

        /**
         * Gets if a format has a depth component.
         * @param format The format to check.
         * @return If the format has a depth component.
         */
        static bool HasStencil(VkFormat format);

        static void CreateImage(VkImage &image, VmaAllocation &allocation, const UVec3 &extent, VkFormat format, VkSampleCountFlagBits samples,
                                VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels, uint32_t arrayLayers, VkImageType type);
        static void CreateImageSampler(VkSampler &sampler, VkFilter filter, VkSamplerAddressMode addressMode, bool anisotropic, uint32_t mipLevels);
        static void CreateImageView(const VkImage &image, VkImageView &imageView, VkImageViewType type, VkFormat format, VkImageAspectFlags imageAspect,
                                    uint32_t mipLevels, uint32_t baseMipLevel, uint32_t layerCount, uint32_t baseArrayLayer);
        static void CreateMipmaps(const VkImage &image, const UVec3 &extent, VkFormat format, VkImageLayout dstImageLayout, uint32_t mipLevels,
                                  uint32_t baseArrayLayer, uint32_t layerCount);
        static void TransitionImageLayout(const VkImage &image, VkFormat format, VkImageLayout srcImageLayout, VkImageLayout dstImageLayout,
                                          VkImageAspectFlags imageAspect, uint32_t mipLevels, uint32_t baseMipLevel, uint32_t layerCount, uint32_t baseArrayLayer);
        static void InsertImageMemoryBarrier(const CommandBuffer &commandBuffer, const VkImage &image, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                             VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                             VkImageAspectFlags imageAspect, uint32_t mipLevels, uint32_t baseMipLevel, uint32_t layerCount, uint32_t baseArrayLayer);
        static void CopyBufferToImage(const VkBuffer &buffer, const VkImage &image, const UVec3 &extent, uint32_t layerCount, uint32_t baseArrayLayer);
        static bool CopyImage(const VkImage &srcImage, VkImage &dstImage, VmaAllocation &alloc, VkFormat srcFormat, const UVec3 &extent,
                              VkImageLayout srcImageLayout, uint32_t mipLevel, uint32_t arrayLayer);

        void Serialize(XMLNode &node) const override;
        void Deserialize(const XMLNode &node) override;

    protected:
        UVec3 extent;
        VkSampleCountFlagBits samples;
        VkImageUsageFlags usage;
        VkFormat format = VK_FORMAT_UNDEFINED;
        uint32_t mipLevels = 0;
        uint32_t arrayLayers;

        VkFilter filter;
        VkSamplerAddressMode addressMode;

        VkImageLayout layout;

        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;

    public:
        static void EnsureReflected()
        {
            static bool reflected = []
            {
                auto &ctx = ::SF::RTTI::SerializeContext::Instance();

                ctx.Class<VkExtent3D>()
                    ->Version(1)
                    ->Field("width", &VkExtent3D::width)
                    ->Field("height", &VkExtent3D::height)
                    ->Field("depth", &VkExtent3D::depth);

                ctx.Class<Image>()
                    ->Version(1)
                    ->Field("Extent", &Image::extent)
                    ->Field("format", &Image::format)
                    ->Field("samples", &Image::samples)
                    ->Field("usage", &Image::usage)
                    ->Field("mipLevels", &Image::mipLevels)
                    ->Field("arrayLayers", &Image::arrayLayers)
                    ->Field("filter", &Image::filter)
                    ->Field("addressMode", &Image::addressMode)
                    ->Field("layout", &Image::layout);

                return true;
            }();
            (void)reflected;
        }
    };

    inline void ImageBarrier(VkCommandBuffer cmd,
                             VkImage image,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             VkAccessFlags srcAccess,
                             VkAccessFlags dstAccess,
                             VkPipelineStageFlags srcStage,
                             VkPipelineStageFlags dstStage,
                             VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                             uint32_t mipLevel = 0,
                             uint32_t arrayLayer = 0)
    {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = oldLayout;
        b.newLayout = newLayout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange.aspectMask = aspect;
        b.subresourceRange.baseMipLevel = mipLevel;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.baseArrayLayer = arrayLayer;
        b.subresourceRange.layerCount = 1;
        b.srcAccessMask = srcAccess;
        b.dstAccessMask = dstAccess;
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                             0, nullptr, 0, nullptr, 1, &b);
    }
    inline void ImageArrayBarrier(VkCommandBuffer cmd,
                                  VkImage image,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout,
                                  VkAccessFlags srcAccess,
                                  VkAccessFlags dstAccess,
                                  VkPipelineStageFlags srcStage,
                                  VkPipelineStageFlags dstStage,
                                  uint32_t layerCount,
                                  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
    {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout = oldLayout;
        b.newLayout = newLayout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange.aspectMask = aspect;
        b.subresourceRange.baseMipLevel = 0;
        b.subresourceRange.levelCount = 1;
        b.subresourceRange.baseArrayLayer = 0;
        b.subresourceRange.layerCount = layerCount;
        b.srcAccessMask = srcAccess;
        b.dstAccessMask = dstAccess;
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                             0, nullptr, 0, nullptr, 1, &b);
    }
}