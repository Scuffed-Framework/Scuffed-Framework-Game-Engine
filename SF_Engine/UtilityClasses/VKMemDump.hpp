#pragma once

//  Quick start:
//    VkDump::Dumper dump(device, physicalDevice, queue, queueFamilyIndex, commandPool);
//
//    // Dump a texture as PNG
//    dump.dumpImage(myTexture, VK_FORMAT_R8G8B8A8_UNORM, 512, 512,
//                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, "out/texture.png");
//
//    // Dump a compute output buffer as CSV floats
//    dump.dumpBuffer(myBuffer, numElements * sizeof(float),
//                   VkDump::Format::Float32, "out/compute_out.csv");
//
//    // Dump raw device memory
//    dump.dumpMemory(myMemory, 0, byteSize, "out/raw.bin");

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <Graphics/RenderSystem.hpp>
#include <Engine/Engine.hpp>

#ifdef VK_DUMP_STB_IMAGE_WRITE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

namespace SF::Engine::VkMemDump
{

    //  helpers

#define VK_DUMP_CHECK(expr)                                      \
    do                                                           \
    {                                                            \
        VkResult _r = (expr);                                    \
        if (_r != VK_SUCCESS)                                    \
        {                                                        \
            std::ostringstream _ss;                              \
            _ss << "[vk_dump] " #expr " failed (VkResult=" << _r \
                << ") at " << __FILE__ << ":" << __LINE__;       \
            throw std::runtime_error(_ss.str());                 \
        }                                                        \
    } while (0)

    // How to interpret raw bytes in a buffer dump
    enum class Format
    {
        Raw, // hex dump
        Uint8,
        Uint16,
        Uint32,
        Float32,
        Float16,  // packed as uint16, decoded to float
        R8G8B8A8, // colour quads → PNG (image path required)
        D32Float, // depth buffer
        BC1,
        BC3,
        BC5,
        BC7, // block-compressed (raw dump only, no decode)
    };

    //  format metadata

    struct FormatInfo
    {
        const char *name;
        uint32_t bytesPerPixel; // 0 = variable / block
        bool isDepth;
        bool isFloat;
        bool isCompressed;
    };

    inline FormatInfo vkFormatInfo(VkFormat f)
    {
        switch (f)
        {
        case VK_FORMAT_R8_UNORM:
            return {"R8_UNORM", 1, false, false, false};
        case VK_FORMAT_R8G8_UNORM:
            return {"R8G8_UNORM", 2, false, false, false};
        case VK_FORMAT_R8G8B8_UNORM:
            return {"R8G8B8_UNORM", 3, false, false, false};
        case VK_FORMAT_R8G8B8A8_UNORM:
            return {"R8G8B8A8_UNORM", 4, false, false, false};
        case VK_FORMAT_R8G8B8A8_SRGB:
            return {"R8G8B8A8_SRGB", 4, false, false, false};
        case VK_FORMAT_B8G8R8A8_UNORM:
            return {"B8G8R8A8_UNORM", 4, false, false, false};
        case VK_FORMAT_B8G8R8A8_SRGB:
            return {"B8G8R8A8_SRGB", 4, false, false, false};
        case VK_FORMAT_R16_SFLOAT:
            return {"R16_SFLOAT", 2, false, true, false};
        case VK_FORMAT_R16G16_SFLOAT:
            return {"R16G16_SFLOAT", 4, false, true, false};
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return {"R16G16B16A16_SFLOAT", 8, false, true, false};
        case VK_FORMAT_R32_SFLOAT:
            return {"R32_SFLOAT", 4, false, true, false};
        case VK_FORMAT_R32G32_SFLOAT:
            return {"R32G32_SFLOAT", 8, false, true, false};
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return {"R32G32B32A32_SFLOAT", 16, false, true, false};
        case VK_FORMAT_R32_UINT:
            return {"R32_UINT", 4, false, false, false};
        case VK_FORMAT_R32G32B32A32_UINT:
            return {"R32G32B32A32_UINT", 16, false, false, false};
        case VK_FORMAT_D16_UNORM:
            return {"D16_UNORM", 2, true, false, false};
        case VK_FORMAT_D32_SFLOAT:
            return {"D32_SFLOAT", 4, true, true, false};
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return {"D24_UNORM_S8_UINT", 4, true, false, false};
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
            return {"BC1_RGB_UNORM", 0, false, false, true};
        case VK_FORMAT_BC3_UNORM_BLOCK:
            return {"BC3_UNORM", 0, false, false, true};
        case VK_FORMAT_BC5_UNORM_BLOCK:
            return {"BC5_UNORM", 0, false, false, true};
        case VK_FORMAT_BC7_UNORM_BLOCK:
            return {"BC7_UNORM", 0, false, false, true};
        default:
            return {"UNKNOWN", 0, false, false, false};
        }
    }

    // half-float decode
    inline float f16ToF32(uint16_t h)
    {
        uint32_t sign = (h >> 15) & 1;
        uint32_t exponent = (h >> 10) & 0x1F;
        uint32_t mantissa = h & 0x3FF;
        uint32_t result;
        if (exponent == 0)
        {
            if (mantissa == 0)
            {
                result = sign << 31;
            }
            else
            {
                exponent = 1;
                while (!(mantissa & 0x400))
                {
                    mantissa <<= 1;
                    exponent--;
                }
                mantissa &= 0x3FF;
                result = (sign << 31) | ((exponent + 112) << 23) | (mantissa << 13);
            }
        }
        else if (exponent == 31)
        {
            result = (sign << 31) | 0x7F800000 | (mantissa << 13);
        }
        else
        {
            result = (sign << 31) | ((exponent + 112) << 23) | (mantissa << 13);
        }
        float f;
        std::memcpy(&f, &result, 4);
        return f;
    }

    //  Dumper

    class Dumper
    {
    public:
        // pass in your existing device objects; the Dumper borrows them (no ownership)
        Dumper(
            VkDevice device = RenderSystem::Get()->GetLogicalDevice()->GetLogicalDevice(),
            VkPhysicalDevice physicalDevice = RenderSystem::Get()->GetPhysicalDevice()->GetPhysicalDevice(),
            VkQueue queue = RenderSystem::Get()->GetLogicalDevice()->GetGraphicsQueue(),
            VkCommandPool commandPool = RenderSystem::Get()->GetCommandPool(::SF::Engine::Engine::Get()->GetRenderThreadId())->GetCommandPool(), // well now we can't dump from another thread, but who cares
            bool verbose = true)
            : device_(device), physDevice_(physicalDevice),
              queue_(queue), cmdPool_(commandPool), verbose_(verbose)
        {
        }

        //  primary entrypoints

        // Dump a VkImage to file.
        //   path extension determines format: .png (needs STB), .ppm, .bin (raw), .csv, .exr (HDR)
        //   currentLayout: the layout the image is currently in before the dump
        void dumpImage(VkImage image,
                       VkFormat format,
                       uint32_t width,
                       uint32_t height,
                       VkImageLayout currentLayout,
                       std::string_view path,
                       uint32_t mipLevel = 0,
                       uint32_t arrayLayer = 0,
                       VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
        {
            auto fi = vkFormatInfo(format);
            log("dumpImage: " + std::string(fi.name) +
                " [" + std::to_string(width) + "x" + std::to_string(height) + "]"
                                                                              " → " +
                std::string(path));

            if (fi.isCompressed)
            {
                log("  (compressed format  raw block dump only)");
            }

            uint32_t bpp = fi.bytesPerPixel ? fi.bytesPerPixel : 8; // BC: 8 bytes/block
            VkDeviceSize byteSize = fi.isCompressed
                                        ? ((width + 3) / 4) * ((height + 3) / 4) * bpp
                                        : (VkDeviceSize)width * height * (fi.bytesPerPixel ? fi.bytesPerPixel : 4);

            // Allocate staging buffer
            auto [stagingBuf, stagingMem] = createStagingBuffer(byteSize);

            // Record and submit copy commands
            VkCommandBuffer cmd = beginOneShot();

            // Transition image to TRANSFER_SRC
            if (currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            {
                imageBarrier(cmd, image,
                             currentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                             VK_ACCESS_TRANSFER_READ_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             aspect, mipLevel, arrayLayer);
            }

            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = aspect;
            region.imageSubresource.mipLevel = mipLevel;
            region.imageSubresource.baseArrayLayer = arrayLayer;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = {width, height, 1};

            vkCmdCopyImageToBuffer(cmd, image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   stagingBuf, 1, &region);

            // Restore original layout
            if (currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            {
                imageBarrier(cmd, image,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, currentLayout,
                             VK_ACCESS_TRANSFER_READ_BIT,
                             VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             aspect, mipLevel, arrayLayer);
            }

            endOneShot(cmd);

            // Map staging buffer and save
            void *data = nullptr;
            VK_DUMP_CHECK(vkMapMemory(device_, stagingMem, 0, byteSize, 0, &data));

            auto ext = std::filesystem::path(path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            ensureDir(path);

            if (ext == ".bin")
                saveBin(data, byteSize, path);
            else if (ext == ".csv" || ext == ".txt")
                saveCSV(data, byteSize, format, width, height, path);
            else if (ext == ".ppm")
                savePPM(data, format, width, height, path);
            else if (ext == ".png")
                savePNG(data, format, width, height, path);
            else if (ext == ".exr")
                saveEXR(data, format, width, height, path);
            else
            {
                // default: try PNG, fallback bin
                log("  unknown extension '" + ext + "', defaulting to .bin");
                saveBin(data, byteSize, path);
            }

            vkUnmapMemory(device_, stagingMem);
            vkDestroyBuffer(device_, stagingBuf, nullptr);
            vkFreeMemory(device_, stagingMem, nullptr);
        }

        // Dump a VkBuffer (SSBO, UBO, vertex buffer, etc.)
        void dumpBuffer(VkBuffer buffer,
                        VkDeviceSize size,
                        Format fmt,
                        std::string_view path,
                        VkDeviceSize offset = 0)
        {
            log("dumpBuffer: " + std::to_string(size) + " bytes → " + std::string(path));

            auto [stagingBuf, stagingMem] = createStagingBuffer(size);

            VkCommandBuffer cmd = beginOneShot();

            // Memory barrier to ensure writes are visible
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT |
                                    VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 0, 1, &barrier, 0, nullptr, 0, nullptr);

            VkBufferCopy copy{offset, 0, size};
            vkCmdCopyBuffer(cmd, buffer, stagingBuf, 1, &copy);

            endOneShot(cmd);

            void *data = nullptr;
            VK_DUMP_CHECK(vkMapMemory(device_, stagingMem, 0, size, 0, &data));

            ensureDir(path);
            auto ext = std::filesystem::path(path).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (ext == ".bin")
            {
                saveBin(data, size, path);
            }
            else
            {
                saveBufferFormatted(data, size, fmt, path);
            }

            vkUnmapMemory(device_, stagingMem);
            vkDestroyBuffer(device_, stagingBuf, nullptr);
            vkFreeMemory(device_, stagingMem, nullptr);
        }

        // Dump raw VkDeviceMemory (must be host-visible, or use dumpBuffer for device-local)
        void dumpMemory(VkDeviceMemory memory,
                        VkDeviceSize offset,
                        VkDeviceSize size,
                        std::string_view path)
        {
            log("dumpMemory: " + std::to_string(size) + " bytes @ +" +
                std::to_string(offset) + " → " + std::string(path));

            void *data = nullptr;
            VK_DUMP_CHECK(vkMapMemory(device_, memory, offset, size, 0, &data));

            ensureDir(path);
            saveBin(data, size, path);
            // Also save hex sidecar
            auto hexPath = std::string(path) + ".hex.txt";
            saveHexDump(data, size, hexPath);

            vkUnmapMemory(device_, memory);
        }

        // Convenience: dump multiple mip levels of a texture
        void dumpImageMips(VkImage image,
                           VkFormat format,
                           uint32_t baseWidth,
                           uint32_t baseHeight,
                           uint32_t mipCount,
                           VkImageLayout currentLayout,
                           std::string_view baseDir,
                           std::string_view baseName)
        {
            for (uint32_t mip = 0; mip < mipCount; ++mip)
            {
                uint32_t w = std::max(1u, baseWidth >> mip);
                uint32_t h = std::max(1u, baseHeight >> mip);
                std::string outPath = std::string(baseDir) + "/" +
                                      std::string(baseName) + "_mip" +
                                      std::to_string(mip) + ".png";
                dumpImage(image, format, w, h, currentLayout, outPath, mip);
            }
        }

        // Print a formatted summary of a format's properties
        static void printFormatInfo(VkFormat fmt)
        {
            auto fi = vkFormatInfo(fmt);
            std::cout << "[vk_dump] format=" << fi.name
                      << " bpp=" << fi.bytesPerPixel
                      << " depth=" << fi.isDepth
                      << " float=" << fi.isFloat
                      << " compressed=" << fi.isCompressed << "\n";
        }

        //  channel statistics
        // After a dump, compute per-channel min/max/mean from the staging data.
        // Useful for spotting all-zero or saturated buffers at a glance.
        struct ChannelStats
        {
            float minVal, maxVal, mean;
            uint64_t zeroCount, nanCount;
        };

        std::vector<ChannelStats> computeImageStats(VkImage image,
                                                    VkFormat format,
                                                    uint32_t width,
                                                    uint32_t height,
                                                    VkImageLayout currentLayout)
        {
            auto fi = vkFormatInfo(format);
            uint32_t channels = std::max(1u, fi.bytesPerPixel / 4);
            VkDeviceSize byteSize = (VkDeviceSize)width * height * fi.bytesPerPixel;

            auto [stagingBuf, stagingMem] = createStagingBuffer(byteSize);
            VkCommandBuffer cmd = beginOneShot();

            if (currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                imageBarrier(cmd, image, currentLayout,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                             VK_ACCESS_TRANSFER_READ_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT);

            VkBufferImageCopy r{};
            r.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            r.imageSubresource.layerCount = 1;
            r.imageExtent = {width, height, 1};
            vkCmdCopyImageToBuffer(cmd, image,
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   stagingBuf, 1, &r);

            if (currentLayout != VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                imageBarrier(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             currentLayout,
                             VK_ACCESS_TRANSFER_READ_BIT,
                             VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

            endOneShot(cmd);

            void *data = nullptr;
            VK_DUMP_CHECK(vkMapMemory(device_, stagingMem, 0, byteSize, 0, &data));

            std::vector<ChannelStats> stats(channels, {1e38f, -1e38f, 0.f, 0, 0});
            uint32_t numPixels = width * height;
            const float *fp = reinterpret_cast<const float *>(data);

            if (fi.isFloat && fi.bytesPerPixel == 4)
            {
                // R32_SFLOAT, single channel
                for (uint32_t i = 0; i < numPixels; ++i)
                {
                    float v = fp[i];
                    if (std::isnan(v))
                    {
                        stats[0].nanCount++;
                        continue;
                    }
                    if (v == 0.f)
                        stats[0].zeroCount++;
                    stats[0].minVal = std::min(stats[0].minVal, v);
                    stats[0].maxVal = std::max(stats[0].maxVal, v);
                    stats[0].mean += v;
                }
                stats[0].mean /= numPixels;
            }
            else
            {
                // RGBA8 and friends  treat as uint8 channels
                const uint8_t *bp = reinterpret_cast<const uint8_t *>(data);
                for (uint32_t i = 0; i < numPixels; ++i)
                {
                    for (uint32_t c = 0; c < channels && c < 4; ++c)
                    {
                        float v = bp[i * fi.bytesPerPixel + c] / 255.f;
                        if (v == 0.f)
                            stats[c].zeroCount++;
                        stats[c].minVal = std::min(stats[c].minVal, v);
                        stats[c].maxVal = std::max(stats[c].maxVal, v);
                        stats[c].mean += v;
                    }
                }
                for (auto &s : stats)
                    s.mean /= numPixels;
            }

            vkUnmapMemory(device_, stagingMem);
            vkDestroyBuffer(device_, stagingBuf, nullptr);
            vkFreeMemory(device_, stagingMem, nullptr);

            if (verbose_)
            {
                const char *chName[] = {"R", "G", "B", "A"};
                for (uint32_t c = 0; c < channels; ++c)
                {
                    std::cout << "[vk_dump]   ch=" << chName[c]
                              << " min=" << stats[c].minVal
                              << " max=" << stats[c].maxVal
                              << " mean=" << stats[c].mean
                              << " zeros=" << stats[c].zeroCount
                              << " nans=" << stats[c].nanCount << "\n";
                }
            }
            return stats;
        }

    private:
        VkDevice device_;
        VkPhysicalDevice physDevice_;
        VkQueue queue_;
        uint32_t queueFamily_;
        VkCommandPool cmdPool_;
        bool verbose_;

        void log(const std::string &msg) const
        {
            if (verbose_)
                std::cout << "[vk_dump] " << msg << "\n";
        }

        //  Vulkan helpers

        uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props)
        {
            VkPhysicalDeviceMemoryProperties mp{};
            vkGetPhysicalDeviceMemoryProperties(physDevice_, &mp);
            for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
                if ((typeBits & (1u << i)) &&
                    (mp.memoryTypes[i].propertyFlags & props) == props)
                    return i;
            throw std::runtime_error("[vk_dump] no suitable memory type");
        }

        std::pair<VkBuffer, VkDeviceMemory> createStagingBuffer(VkDeviceSize size)
        {
            VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bci.size = size;
            bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VkBuffer buf;
            VK_DUMP_CHECK(vkCreateBuffer(device_, &bci, nullptr, &buf));

            VkMemoryRequirements req{};
            vkGetBufferMemoryRequirements(device_, buf, &req);

            VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            ai.allocationSize = req.size;
            ai.memoryTypeIndex = findMemoryType(req.memoryTypeBits,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            VkDeviceMemory mem;
            VK_DUMP_CHECK(vkAllocateMemory(device_, &ai, nullptr, &mem));
            VK_DUMP_CHECK(vkBindBufferMemory(device_, buf, mem, 0));
            return {buf, mem};
        }

        VkCommandBuffer beginOneShot()
        {
            VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
            ai.commandPool = cmdPool_;
            ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandBufferCount = 1;

            VkCommandBuffer cmd;
            VK_DUMP_CHECK(vkAllocateCommandBuffers(device_, &ai, &cmd));

            VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_DUMP_CHECK(vkBeginCommandBuffer(cmd, &bi));
            return cmd;
        }

        void endOneShot(VkCommandBuffer cmd)
        {
            VK_DUMP_CHECK(vkEndCommandBuffer(cmd));

            VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            si.commandBufferCount = 1;
            si.pCommandBuffers = &cmd;

            VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            VkFence fence;
            VK_DUMP_CHECK(vkCreateFence(device_, &fi, nullptr, &fence));
            VK_DUMP_CHECK(vkQueueSubmit(queue_, 1, &si, fence));
            VK_DUMP_CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX));
            vkDestroyFence(device_, fence, nullptr);
            vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
        }

        void imageBarrier(VkCommandBuffer cmd,
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

        //  file writers

        static void ensureDir(std::string_view path)
        {
            auto dir = std::filesystem::path(path).parent_path();
            if (!dir.empty())
                std::filesystem::create_directories(dir);
        }

        static void saveBin(const void *data, VkDeviceSize size, std::string_view path)
        {
            std::ofstream f{std::string(path), std::ios::binary};
            if (!f)
                throw std::runtime_error("[vk_dump] cannot open " + std::string(path));
            f.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
            std::cout << "[vk_dump]   wrote " << size << " bytes → " << path << "\n";
        }

        static void saveHexDump(const void *data, VkDeviceSize size, const std::string &path)
        {
            std::ofstream f(path);
            const uint8_t *p = reinterpret_cast<const uint8_t *>(data);
            constexpr int W = 16;
            for (VkDeviceSize i = 0; i < size; i += W)
            {
                f << std::setw(8) << std::setfill('0') << std::hex << i << "  ";
                for (int j = 0; j < W; ++j)
                {
                    if (i + j < size)
                        f << std::setw(2) << (int)p[i + j] << " ";
                    else
                        f << "   ";
                    if (j == 7)
                        f << " ";
                }
                f << " |";
                for (int j = 0; j < W && i + j < size; ++j)
                    f << (char)(p[i + j] >= 32 && p[i + j] < 127 ? p[i + j] : '.');
                f << "|\n";
            }
            std::cout << "[vk_dump]   hex dump → " << path << "\n";
        }

        static void saveCSV(const void *data, VkDeviceSize size, VkFormat format,
                            uint32_t width, uint32_t height, std::string_view path)
        {
            auto fi = vkFormatInfo(format);
            std::ofstream f{std::string(path)};
            if (!f)
                throw std::runtime_error("[vk_dump] cannot open " + std::string(path));

            f << "# format=" << fi.name
              << " width=" << width << " height=" << height << "\n";

            if (fi.isFloat && fi.bytesPerPixel == 4)
            {
                // R32_SFLOAT
                f << "x,y,value\n";
                const float *fp = reinterpret_cast<const float *>(data);
                for (uint32_t y = 0; y < height; ++y)
                    for (uint32_t x = 0; x < width; ++x)
                        f << x << "," << y << "," << fp[y * width + x] << "\n";
            }
            else if (fi.isFloat && fi.bytesPerPixel == 16)
            {
                // R32G32B32A32_SFLOAT
                f << "x,y,r,g,b,a\n";
                const float *fp = reinterpret_cast<const float *>(data);
                for (uint32_t y = 0; y < height; ++y)
                    for (uint32_t x = 0; x < width; ++x)
                    {
                        const float *px = fp + (y * width + x) * 4;
                        f << x << "," << y << ","
                          << px[0] << "," << px[1] << ","
                          << px[2] << "," << px[3] << "\n";
                    }
            }
            else if (fi.bytesPerPixel == 4 && !fi.isFloat)
            {
                // RGBA8
                f << "x,y,r,g,b,a\n";
                const uint8_t *bp = reinterpret_cast<const uint8_t *>(data);
                for (uint32_t y = 0; y < height; ++y)
                    for (uint32_t x = 0; x < width; ++x)
                    {
                        const uint8_t *px = bp + (y * width + x) * 4;
                        f << x << "," << y << ","
                          << (int)px[0] << "," << (int)px[1] << ","
                          << (int)px[2] << "," << (int)px[3] << "\n";
                    }
            }
            else
            {
                // Generic hex dump as CSV
                f << "offset,hex\n";
                const uint8_t *bp = reinterpret_cast<const uint8_t *>(data);
                for (VkDeviceSize i = 0; i < size; ++i)
                    f << i << "," << std::hex << (int)bp[i] << "\n";
            }
            std::cout << "[vk_dump]   CSV → " << path << "\n";
        }

        static void savePPM(const void *data, VkFormat format,
                            uint32_t width, uint32_t height, std::string_view path)
        {
            auto fi = vkFormatInfo(format);
            std::vector<uint8_t> rgb(width * height * 3);
            const uint8_t *src = reinterpret_cast<const uint8_t *>(data);

            if (fi.bytesPerPixel == 4 && !fi.isFloat)
            {
                // RGBA8 or BGRA8
                bool bgr = (format == VK_FORMAT_B8G8R8A8_UNORM ||
                            format == VK_FORMAT_B8G8R8A8_SRGB);
                for (uint32_t i = 0; i < width * height; ++i)
                {
                    rgb[i * 3 + 0] = bgr ? src[i * 4 + 2] : src[i * 4 + 0];
                    rgb[i * 3 + 1] = src[i * 4 + 1];
                    rgb[i * 3 + 2] = bgr ? src[i * 4 + 0] : src[i * 4 + 2];
                }
            }
            else if (fi.isFloat && fi.bytesPerPixel == 4)
            {
                // D32 or R32F  normalize to 0-255
                const float *fp = reinterpret_cast<const float *>(data);
                float mn = *fp, mx = *fp;
                for (uint32_t i = 1; i < width * height; ++i)
                {
                    mn = std::min(mn, fp[i]);
                    mx = std::max(mx, fp[i]);
                }
                float range = mx - mn < 1e-6f ? 1.f : mx - mn;
                for (uint32_t i = 0; i < width * height; ++i)
                {
                    uint8_t v = (uint8_t)(((fp[i] - mn) / range) * 255.f);
                    rgb[i * 3 + 0] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = v;
                }
            }
            else
            {
                // Fallback: just copy first 3 bytes per pixel
                uint32_t bpp = std::max(3u, fi.bytesPerPixel);
                for (uint32_t i = 0; i < width * height; ++i)
                {
                    rgb[i * 3 + 0] = i * bpp + 0 < (uint32_t)(width * height * bpp) ? src[i * bpp + 0] : 0;
                    rgb[i * 3 + 1] = i * bpp + 1 < (uint32_t)(width * height * bpp) ? src[i * bpp + 1] : 0;
                    rgb[i * 3 + 2] = i * bpp + 2 < (uint32_t)(width * height * bpp) ? src[i * bpp + 2] : 0;
                }
            }

            std::ofstream f(std::string(path), std::ios::binary);
            if (!f)
                throw std::runtime_error("[vk_dump] cannot open " + std::string(path));
            f << "P6\n"
              << width << " " << height << "\n255\n";
            f.write(reinterpret_cast<const char *>(rgb.data()), rgb.size());
            std::cout << "[vk_dump]   PPM → " << path << "\n";
        }

        static void savePNG(const void *data, VkFormat format,
                            uint32_t width, uint32_t height, std::string_view path)
        {
#ifdef VK_DUMP_STB_IMAGE_WRITE
            auto fi = vkFormatInfo(format);
            if (fi.bytesPerPixel == 4 && !fi.isFloat)
            {
                bool bgr = (format == VK_FORMAT_B8G8R8A8_UNORM ||
                            format == VK_FORMAT_B8G8R8A8_SRGB);
                const uint8_t *src = reinterpret_cast<const uint8_t *>(data);
                if (bgr)
                {
                    std::vector<uint8_t> rgba(width * height * 4);
                    for (uint32_t i = 0; i < width * height; ++i)
                    {
                        rgba[i * 4 + 0] = src[i * 4 + 2];
                        rgba[i * 4 + 1] = src[i * 4 + 1];
                        rgba[i * 4 + 2] = src[i * 4 + 0];
                        rgba[i * 4 + 3] = src[i * 4 + 3];
                    }
                    stbi_write_png(std::string(path).c_str(), width, height, 4, rgba.data(), width * 4);
                }
                else
                {
                    stbi_write_png(std::string(path).c_str(), width, height, 4, src, width * 4);
                }
            }
            else if (fi.isFloat && fi.bytesPerPixel == 4)
            {
                // Normalize float → grayscale PNG
                const float *fp = reinterpret_cast<const float *>(data);
                float mn = fp[0], mx = fp[0];
                for (uint32_t i = 1; i < width * height; ++i)
                {
                    if (!std::isnan(fp[i]))
                    {
                        mn = std::min(mn, fp[i]);
                        mx = std::max(mx, fp[i]);
                    }
                }
                float range = mx - mn < 1e-6f ? 1.f : mx - mn;
                std::vector<uint8_t> g(width * height);
                for (uint32_t i = 0; i < width * height; ++i)
                    g[i] = std::isnan(fp[i]) ? 0 : (uint8_t)(((fp[i] - mn) / range) * 255.f);
                stbi_write_png(std::string(path).c_str(), width, height, 1, g.data(), width);
            }
            else
            {
                std::cout << "[vk_dump]   PNG: unsupported format, falling back to PPM\n";
                std::string ppm = std::string(path);
                ppm.replace(ppm.rfind(".png"), 4, ".ppm");
                savePPM(data, format, width, height, ppm);
                return;
            }
            std::cout << "[vk_dump]   PNG → " << path << "\n";
#else
            std::cout << "[vk_dump]   PNG requested but VK_DUMP_STB_IMAGE_WRITE not defined; "
                         "writing PPM instead\n";
            std::string ppm = std::string(path);
            auto pos = ppm.rfind(".png");
            if (pos != std::string::npos)
                ppm.replace(pos, 4, ".ppm");
            savePPM(data, format, width, height, ppm);
#endif
        }

        // Minimal EXR writer (single-channel HALF or FLOAT, no compression)
        static void saveEXR(const void *data, VkFormat format,
                            uint32_t width, uint32_t height, std::string_view path)
        {
            // Minimal OpenEXR-compatible binary (flat uncompressed scanlines)
            // Full spec: https://openexr.com/documentation/openexrfilelayout.pdf
            // For production use, link against OpenEXR or tinyexr.
            std::cout << "[vk_dump]   EXR: minimal writer  falling back to .bin for now.\n"
                         "            Link tinyexr (https://github.com/syoyo/tinyexr) and\n"
                         "            call TinyExr::SaveEXR() for full HDR output.\n";
            std::string binPath = std::string(path) + ".bin";
            auto fi = vkFormatInfo(format);
            uint32_t bpp = fi.bytesPerPixel ? fi.bytesPerPixel : 4;
            saveBin(data, (VkDeviceSize)width * height * bpp, binPath);
        }

        void saveBufferFormatted(const void *data, VkDeviceSize size,
                                 Format fmt, std::string_view path)
        {
            std::ofstream f{std::string(path)};
            if (!f)
                throw std::runtime_error("[vk_dump] cannot open " + std::string(path));

            switch (fmt)
            {
            case Format::Uint8:
            {
                f << "# uint8\nindex,value\n";
                const uint8_t *p = reinterpret_cast<const uint8_t *>(data);
                for (VkDeviceSize i = 0; i < size; ++i)
                    f << i << "," << (int)p[i] << "\n";
                break;
            }
            case Format::Uint32:
            {
                f << "# uint32\nindex,value\n";
                const uint32_t *p = reinterpret_cast<const uint32_t *>(data);
                VkDeviceSize n = size / 4;
                for (VkDeviceSize i = 0; i < n; ++i)
                    f << i << "," << p[i] << "\n";
                break;
            }
            case Format::Float32:
            {
                f << "# float32\nindex,value\n";
                const float *p = reinterpret_cast<const float *>(data);
                VkDeviceSize n = size / 4;
                f << std::setprecision(8);
                for (VkDeviceSize i = 0; i < n; ++i)
                    f << i << "," << p[i] << "\n";
                break;
            }
            case Format::Float16:
            {
                f << "# float16 (decoded)\nindex,value\n";
                const uint16_t *p = reinterpret_cast<const uint16_t *>(data);
                VkDeviceSize n = size / 2;
                for (VkDeviceSize i = 0; i < n; ++i)
                    f << i << "," << f16ToF32(p[i]) << "\n";
                break;
            }
            case Format::D32Float:
            {
                f << "# D32_SFLOAT depth\nindex,depth\n";
                const float *p = reinterpret_cast<const float *>(data);
                VkDeviceSize n = size / 4;
                for (VkDeviceSize i = 0; i < n; ++i)
                    f << i << "," << p[i] << "\n";
                break;
            }
            default:
            {
                // hex
                f << "# raw hex\noffset,hex\n";
                const uint8_t *p = reinterpret_cast<const uint8_t *>(data);
                for (VkDeviceSize i = 0; i < size; ++i)
                    f << i << "," << std::hex << (int)p[i] << "\n";
                break;
            }
            }
            std::cout << "[vk_dump]   buffer CSV → " << path << "\n";
        }
    };

} // namespace VkDump