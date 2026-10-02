#pragma once

#include <vector>

#include <LowLevel/XML/XMLModule.hpp>
#include <Math/Vectors/Vector.hpp>
#include <Rendering/RHI/Commands/CommandBuffer.hpp>
#include <Rendering/RHI/Descriptors/BasicDescriptor.hpp>
#include <filesystem>
#include <type_traits>
#include <typeindex>
#include <vk_mem_alloc.h>

#include <Assets/Bitmaps/Bitmap.hpp>
#include <LowLevel/XML/XMLNodeWriter.hpp>

#include <Gui/ImGui/ocornut/imgui.h>
#include <Gui/ImGui/ocornut/imgui_impl_vulkan.h>

#include <Assets/AssetPipeline.hpp>

SF_REFLECT_EXTERNAL_TYPE(VkExtent3D)

SF_REFLECT_EXTERNAL_TYPE(VkExtent2D)
SF_REFLECT_EXTERNAL_TYPE(ImTextureID)

namespace SF::Engine
{
    class DescriptorSet;
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
        Image(VkFilter filter, VkSamplerAddressMode addressMode, VkSampleCountFlagBits samples, VkImageLayout layout,
              VkImageUsageFlags usage, VkFormat format, uint32_t mipLevels, uint32_t arrayLayers, const UVec3 &extent);

        ~Image();

        [[nodiscard]] WriteDescriptorSetInformation
        GetWriteDescriptor(uint32_t binding, VkDescriptorType descriptorType,
                           const std::optional<OffsetSize> &offsetSize) const override;
        static VkDescriptorSetLayoutBinding GetDescriptorSetLayout(uint32_t binding, VkDescriptorType descriptorType,
                                                                   VkShaderStageFlags stage, uint32_t count);

        /**
         * Copies the images pixels from memory to a bitmap. If this method is called from multiple threads at the same
         * time Vulkan will crash!
         * @param mipLevel The mipmap level index to sample.
         * @param arrayLayer The array level to sample.
         * @return A copy of the images pixels.
         */
        [[nodiscard]] std::unique_ptr<Bitmap> GetBitmap(uint32_t mipLevel = 0, uint32_t arrayLayer = 0) const;

        [[nodiscard]] const UVec3 &GetExtent() const { return extent; }
        [[nodiscard]] UVec2 GetSize() const { return {extent.x, extent.y}; }
        [[nodiscard]] VkFormat GetFormat() const { return format; }
        [[nodiscard]] VkSampleCountFlagBits GetSamples() const { return samples; }
        [[nodiscard]] VkImageUsageFlags GetUsage() const { return usage; }
        [[nodiscard]] uint32_t GetMipLevels() const { return mipLevels; }
        [[nodiscard]] uint32_t GetArrayLevels() const { return arrayLayers; }
        [[nodiscard]] VkFilter GetFilter() const { return filter; }
        [[nodiscard]] VkSamplerAddressMode GetAddressMode() const { return addressMode; }
        [[nodiscard]] VkImageLayout GetLayout() const { return layout; }
        [[nodiscard]] const VkImage &GetImage() { return image; }
        [[nodiscard]] const VmaAllocation &GetAllocation() { return allocation; }
        [[nodiscard]] const VkSampler &GetSampler() const { return sampler; }
        [[nodiscard]] const VkImageView &GetView() const { return view; }

        static uint32_t GetMipLevels(const UVec3 &extent);

        void SetLayout(VkImageLayout newLayout) { layout = newLayout; }

        /**
         * Find a format in the candidates list that fits the tiling and features required.
         * @param candidates Formats that are tested for features, in order of preference.
         * @param tiling Tiling mode to test features in.
         * @param features The features to test for.
         * @return The format found, or VK_FORMAT_UNDEFINED.
         */
        static VkFormat FindSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling,
                                            VkFormatFeatureFlags features);

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

        static void CreateImage(VkImage &image, VmaAllocation &allocation, const UVec3 &extent, VkFormat format,
                                VkSampleCountFlagBits samples, VkImageTiling tiling, VkImageUsageFlags usage,
                                VkMemoryPropertyFlags properties, uint32_t mipLevels, uint32_t arrayLayers,
                                VkImageType type);
        static void CreateImageSampler(VkSampler &sampler, VkFilter filter, VkSamplerAddressMode addressMode,
                                       bool anisotropic, uint32_t mipLevels);
        static void CreateImageView(const VkImage &image, VkImageView &imageView, VkImageViewType type, VkFormat format,
                                    VkImageAspectFlags imageAspect, uint32_t mipLevels, uint32_t baseMipLevel,
                                    uint32_t layerCount, uint32_t baseArrayLayer);
        static void CreateMipmaps(const VkImage &image, const UVec3 &extent, VkFormat format,
                                  VkImageLayout dstImageLayout, uint32_t mipLevels, uint32_t baseArrayLayer,
                                  uint32_t layerCount);
        static void TransitionImageLayout(const VkImage &image, VkFormat format, VkImageLayout srcImageLayout,
                                          VkImageLayout dstImageLayout, VkImageAspectFlags imageAspect,
                                          uint32_t mipLevels, uint32_t baseMipLevel, uint32_t layerCount,
                                          uint32_t baseArrayLayer);
        static void InsertImageMemoryBarrier(const CommandBuffer &commandBuffer, const VkImage &image,
                                             VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                             VkImageLayout oldImageLayout, VkImageLayout newImageLayout,
                                             VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                                             VkImageAspectFlags imageAspect, uint32_t mipLevels, uint32_t baseMipLevel,
                                             uint32_t layerCount, uint32_t baseArrayLayer);
        static void CopyBufferToImage(const VkBuffer &buffer, const VkImage &image, const UVec3 &extent,
                                      uint32_t layerCount, uint32_t baseArrayLayer);
        static bool CopyImage(const VkImage &srcImage, VkImage &dstImage, VmaAllocation &alloc, VkFormat srcFormat,
                              const UVec3 &extent, VkImageLayout srcImageLayout, uint32_t mipLevel,
                              uint32_t arrayLayer);

        void Serialize(XMLNode &node) const override;
        void Deserialize(const XMLNode &node) override;

    protected:
        UVec3 extent;
        VkSampleCountFlagBits samples;
        VkImageUsageFlags usage;
        VkFormat format    = VK_FORMAT_UNDEFINED;
        uint32_t mipLevels = 0;
        uint32_t arrayLayers;

        VkFilter filter;
        VkSamplerAddressMode addressMode;

        VkImageLayout layout;

        VkImage image            = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkSampler sampler        = VK_NULL_HANDLE;
        VkImageView view         = VK_NULL_HANDLE;

        ImTextureID imguiTexId = {};
        /**
         * @brief Generates an ImTextureId
         */
        void GenerateTexId()
        {
            imguiTexId = (ImTextureID) ImGui_ImplVulkan_AddTexture(GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }

    public:
        /**
         * @brief Get the ImTextureId
         * @return ImTextureId
         */
        [[nodiscard]] ImTextureID GetTexID() const { return imguiTexId; };

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
            (void) reflected;
        }
    };

    inline void ImageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                             VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage,
                             VkPipelineStageFlags dstStage, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                             uint32_t mipLevel = 0, uint32_t arrayLayer = 0)
    {
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        b.oldLayout                       = oldLayout;
        b.newLayout                       = newLayout;
        b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        b.image                           = image;
        b.subresourceRange.aspectMask     = aspect;
        b.subresourceRange.baseMipLevel   = mipLevel;
        b.subresourceRange.levelCount     = 1;
        b.subresourceRange.baseArrayLayer = arrayLayer;
        b.subresourceRange.layerCount     = 1;
        b.srcAccessMask                   = srcAccess;
        b.dstAccessMask                   = dstAccess;
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    inline void ImageArrayBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                                  VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage,
                                  VkPipelineStageFlags dstStage, uint32_t layerCount,
                                  VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
    {
        VkImageMemoryBarrier b{
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask       = srcAccess,
                .dstAccessMask       = dstAccess,
                .oldLayout           = oldLayout,
                .newLayout           = newLayout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image               = image,
        };
        b.subresourceRange.aspectMask = aspect, b.subresourceRange.baseMipLevel = 0, b.subresourceRange.levelCount = 1,
        b.subresourceRange.baseArrayLayer = 0, b.subresourceRange.layerCount = layerCount,

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    class Cubemap : public Image
    {
    public:
        /**
         * Creates a new cubemap image from file.
         * @param filename The file to load the image from.
         * @param fileSuffix The files extension type (ex .png).
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param anisotropic If anisotropic filtering is enabled.
         * @param mipmap If mipmaps will be generated.
         */
        explicit Cubemap(std::filesystem::path filename, std::string fileSuffix = ".png",
                         VkFilter filter                  = VK_FILTER_LINEAR,
                         VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         bool anisotropic = true, bool mipmap = true);

        /**
         * Creates a new empty cubemap image.
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
        explicit Cubemap(const UVec2 &extent, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                         VkImageLayout layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                         VkFilter filter         = VK_FILTER_LINEAR,
                         VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false,
                         bool mipmap = false);

        /**
         * Creates a new cubemap image from bitmap.
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
        explicit Cubemap(std::unique_ptr<Bitmap> &&bitmap, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                         VkImageLayout layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                         VkFilter filter         = VK_FILTER_LINEAR,
                         VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false,
                         bool mipmap = false);

        /**
         * Copies the images pixels from memory to a bitmap. The bitmap height will be scaled by the amount of layers.
         * @param mipLevel The mipmap level index to sample.
         * @return A copy of the images pixels.
         */
        [[nodiscard]] std::unique_ptr<Bitmap> GetBitmap(uint32_t mipLevel = 0) const;

        /**
         * Sets the pixels of this image.
         * @param pixels The pixels to copy from.
         * @param layerCount The amount of layers contained in the pixels.
         * @param baseArrayLayer The first layer to copy into.
         */
        void SetPixels(const uint8_t *pixels, uint32_t layerCount, uint32_t baseArrayLayer);

        [[nodiscard]] const std::filesystem::path &GetFilename() const { return filename; }
        [[nodiscard]] const std::string &GetFileSuffix() const { return fileSuffix; }
        [[nodiscard]] const std::vector<std::string> &GetFileSides() const { return fileSides; }
        [[nodiscard]] bool IsAnisotropic() const { return anisotropic; }
        [[nodiscard]] bool IsMipmap() const { return mipmap; }
        [[nodiscard]] uint32_t GetComponents() const { return components; }

    private:
        void Load(std::unique_ptr<Bitmap> loadBitmap = nullptr);

        std::filesystem::path filename;
        std::string fileSuffix;
        // +X, -X, +Y, -Y, +Z, -Z
        std::vector<std::string> fileSides = {"Right", "Left", "Top", "Bottom", "Back", "Front"};

        bool anisotropic;
        bool mipmap;
        uint32_t components = 0;
    };

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
        static std::shared_ptr<Image2d> Create(const std::filesystem::path &filename,
                                               VkFilter filter                  = VK_FILTER_LINEAR,
                                               VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                               bool anisotropic = true, bool mipmap = true);

        /**
         * Creates a new 2D image from file.
         * @param filename The file to load the image from.
         * @param filter The magnification/minification filter to apply to lookups.
         * @param addressMode The addressing mode for outside [0..1] range.
         * @param anisotropic If anisotropic filtering is enabled.
         * @param mipmap If mipmaps will be generated.
         * @param load If this resource will be loaded immediately, otherwise {@link Image2d#Load\endlink} can be called
         * later.
         */
        explicit Image2d(std::filesystem::path filename, VkFilter filter = VK_FILTER_LINEAR,
                         VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, bool anisotropic = true,
                         bool mipmap = true, bool load = true);

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
        explicit Image2d(const UVec2 &extent, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                         VkImageLayout layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                         VkFilter filter         = VK_FILTER_LINEAR,
                         VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false,
                         bool mipmap = false);

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
        explicit Image2d(std::unique_ptr<Bitmap> &&bitmap, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                         VkImageLayout layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                         VkFilter filter         = VK_FILTER_LINEAR,
                         VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                         VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false,
                         bool mipmap = false);

        /**
         * Sets the pixels of this image.
         * @param pixels The pixels to copy from.
         * @param layerCount The amount of layers contained in the pixels.
         * @param baseArrayLayer The first layer to copy into.
         */
        void SetPixels(const uint8_t *pixels, uint32_t layerCount, uint32_t baseArrayLayer);

        [[nodiscard]] std::type_index GetTypeIndex() const { return typeid(Image2d); }

        [[nodiscard]] const std::filesystem::path &GetFilename() const { return filename; }
        [[nodiscard]] bool IsAnisotropic() const { return anisotropic; }
        [[nodiscard]] bool IsMipmap() const { return mipmap; }
        [[nodiscard]] uint32_t GetComponents() const { return components; }

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
                     VkImageLayout layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     VkFilter filter         = VK_FILTER_LINEAR,
                     VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, bool anisotropic = false,
                     bool mipmap = false);

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
                     VkImageLayout layout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                     VkFilter filter         = VK_FILTER_LINEAR,
                     VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, bool anisotropic = false,
                     bool mipmap = false);

        /**
         * Sets the pixels of a specific layer of this image array.
         * @param pixels The pixels to copy from.
         * @param arrayLayer The layer to copy into.
         */
        void SetPixels(const uint8_t *pixels, uint32_t arrayLayer);

        [[nodiscard]] bool IsAnisotropic() const { return anisotropic; }
        [[nodiscard]] bool IsMipmap() const { return mipmap; }

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
        Image3d(const UVec3 &extent, VkFormat format = VK_FORMAT_R8G8B8A8_UNORM,
                VkImageLayout layout             = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VkImageUsageFlags usage          = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VkFilter filter                  = VK_FILTER_LINEAR,
                VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false, bool mipmap = false);

        /**
         * Creates a 3D image from in-memory voxel data.
         */
        Image3d(const UVec3 &extent, const uint8_t *voxels, size_t voxelSizeBytes,
                VkFormat format                  = VK_FORMAT_R8G8B8A8_UNORM,
                VkImageLayout layout             = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VkImageUsageFlags usage          = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                VkFilter filter                  = VK_FILTER_LINEAR,
                VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT, bool anisotropic = false, bool mipmap = false);

        /**
         * Upload new voxel data into the volume texture.
         */
        void SetPixels3D(const uint8_t *voxels, size_t voxelSizeBytes);

        [[nodiscard]] std::type_index GetTypeIndex() const { return typeid(Image3d); }

        [[nodiscard]] bool IsAnisotropic() const { return anisotropic_; }
        [[nodiscard]] bool IsMipmap() const { return mipmap_; }

        void Serialize(XMLNode &node) const override
        {
            Image::Serialize(node);
            node.SetAttribute("anisotropic", anisotropic_);
            node.SetAttribute("mipmap", mipmap_);
            node.SetAttribute("voxelSize", (int) voxelSize_);
        }

        void Deserialize(const XMLNode &node) override
        {
            Image::Deserialize(node);
            node.GetAttribute("anisotropic", anisotropic_);
            node.GetAttribute("mipmap", mipmap_);
            int vs;
            node.GetAttribute("voxelSize", vs);
            voxelSize_ = (size_t) vs;
        }

    private:
        void InternalCreate();
        void Upload(const uint8_t *voxels, size_t voxelSize);

        bool anisotropic_;
        bool mipmap_;
        size_t voxelSize_ = 0;
    };

    /**
     * @brief Resource that represents a depth stencil image.
     */
    class ImageDepth : public Image
    {
        SF_RTTI(ImageDepth, Image)
    public:
        explicit ImageDepth(const UVec2 &extent, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
    };

    template<typename TImage = Image2d>
    class ImageAsset : public AssetBase
    {
        SF_RTTI(ImageAsset<TImage>, AssetBase)
        static_assert(std::is_base_of_v<Image, TImage>, "ImageAsset<TImage> requires TImage to derive from Image");

    public:
        std::shared_ptr<TImage> texture;
        std::filesystem::path filename; // empty if not disk-backed (e.g. procedural LUTs)

        void Save() override;

        bool Load(std::span<const uint8_t> payload) override
        {
            if (filename.empty())
                return false;

            XMLModule *writer = XMLModule::Get();
            XMLNode root      = writer->GetRootNode();
            AssetBase::Deserialize(root);

            auto bitmap = std::make_unique<Bitmap>(filename);
            if (!bitmap || !*bitmap)
                return false;

            int rawFormat{}, rawLayout{}, rawUsage{}, rawFilter{}, rawAddressMode{};
            root.GetAttribute("Format", rawFormat);
            root.GetAttribute("Layout", rawLayout);
            root.GetAttribute("UsageBits", rawUsage);
            root.GetAttribute("Filter", rawFilter);
            root.GetAttribute("AddressMode", rawAddressMode);

            auto format      = static_cast<VkFormat>(rawFormat);
            auto layout      = static_cast<VkImageLayout>(rawLayout);
            auto usage       = static_cast<VkImageUsageFlags>(rawUsage);
            auto filter      = static_cast<VkFilter>(rawFilter);
            auto addressMode = static_cast<VkSamplerAddressMode>(rawAddressMode);

            if constexpr (std::is_same_v<TImage, Image2d> || std::is_same_v<TImage, Cubemap>)
            {
                texture = std::make_shared<TImage>(std::move(bitmap), format, layout, usage, filter, addressMode);
            } else if constexpr (std::is_same_v<TImage, Image2dArray>)
            {
                int rawLayerCount{1};
                root.GetAttribute("ArrayLayers", rawLayerCount);
                texture = std::make_shared<TImage>(std::move(bitmap), static_cast<uint32_t>(rawLayerCount), format,
                                                   layout, usage, filter, addressMode);
            } else if constexpr (std::is_same_v<TImage, Image3d>)
            {
                // TODO: Saving 3d textures
                return false;
            } else
            {
                static_assert(!sizeof(TImage), "ImageAsset<TImage>::Load: no loading strategy for this TImage");
            }

            return texture != nullptr;
        }

        // Construct the live TImage directly, forwarding whatever ctor args
        // TImage actually needs (extent, format, filter, voxel data, etc).
        // This is what makes ImageAsset<TImage> work uniformly across
        // Image2d/Image3d/Image2dArray without per-type subclasses; each
        // one just gets called with its own natural constructor signature.
        template<typename... Args>
        void Create(Args &&...args)
        {
            texture = std::make_shared<TImage>(std::forward<Args>(args)...);
        }
    };

    inline void BindStorageImage(DescriptorSet &ds, Image *img);

    // 2-D transitions
    inline void TransitionToGeneral(const CommandBuffer &cmd, Image2d *img)
    {
        Image::InsertImageMemoryBarrier(cmd, img->GetImage(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    inline void TransitionToReadOnly(const CommandBuffer &cmd, Image2d *img)
    {
        Image::InsertImageMemoryBarrier(cmd, img->GetImage(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                        VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    // 3-D transitions
    inline void TransitionToGeneral3d(const CommandBuffer &cmd, Image3d *img)
    {
        Image::InsertImageMemoryBarrier(cmd, img->GetImage(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                        VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }

    inline void TransitionToReadOnly3d(const CommandBuffer &cmd, Image3d *img)
    {
        Image::InsertImageMemoryBarrier(cmd, img->GetImage(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                        VK_IMAGE_ASPECT_COLOR_BIT, 1, 0, 1, 0);
    }
} // namespace SF::Engine
