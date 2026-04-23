#pragma once
#include <Bitmaps/Bitmap.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace SF::Engine
{
    // EXR compression methods
    enum class EXRCompression
    {
        NONE = 0,
        RLE = 1,
        ZIPS = 2,
        ZIP = 3,
        PIZ = 4,
        PXR24 = 5,
        B44 = 6,
        B44A = 7,
        DWAA = 8,
        DWAB = 9
    };

    // EXR pixel types
    enum class EXRPixelType
    {
        UINT = 0,
        HALF = 1,
        FLOAT = 2
    };

#pragma pack(push, 1)
    // EXR file header (first part)
    struct EXRHeader
    {
        uint32_t magic;    // 0x01312F76
        uint32_t version;  // Version field with flags
    };

    // EXR attribute header
    struct EXRAttribute
    {
        // Variable length strings followed by type and data
        // Structure: name\0 type\0 size data
    };
#pragma pack(pop)

    constexpr uint32_t EXR_MAGIC = 0x01312F76;
    constexpr uint32_t EXR_VERSION = 2;
    constexpr uint32_t EXR_TILED_FLAG = 0x00000200;
    constexpr uint32_t EXR_LONG_NAMES_FLAG = 0x00000400;
    constexpr uint32_t EXR_MULTIPART_FLAG = 0x00001000;

    class BitmapEXR : public Bitmap::Registrar<BitmapEXR>
    {
    public:
        static void Load(Bitmap& bitmap, const std::filesystem::path& filename)
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to open EXR file: " + filename.string());
            }

            // Read magic number and version
            EXRHeader header;
            file.read(reinterpret_cast<char*>(&header), sizeof(EXRHeader));

            if (header.magic != EXR_MAGIC)
            {
                throw std::runtime_error("Invalid EXR file magic number");
            }

            uint32_t version = header.version & 0xFF;
            uint32_t flags = header.version & 0xFFFFFF00;

            if (flags & EXR_TILED_FLAG)
            {
                throw std::runtime_error("Tiled EXR files not supported");
            }

            if (flags & EXR_MULTIPART_FLAG)
            {
                throw std::runtime_error("Multi-part EXR files not supported");
            }

            // Parse attributes
            int width = 0, height = 0;
            int dataWindowMinX = 0, dataWindowMinY = 0;
            int dataWindowMaxX = 0, dataWindowMaxY = 0;
            EXRCompression compression = EXRCompression::NONE;
            std::vector<std::string> channelNames;
            std::vector<EXRPixelType> channelTypes;

            while (true)
            {
                // Read attribute name
                std::string attrName;
                char ch;
                while (file.get(ch) && ch != '\0')
                {
                    attrName += ch;
                }

                // Empty name means end of header
                if (attrName.empty()) break;

                // Read attribute type
                std::string attrType;
                while (file.get(ch) && ch != '\0')
                {
                    attrType += ch;
                }

                // Read attribute size
                uint32_t attrSize;
                file.read(reinterpret_cast<char*>(&attrSize), sizeof(uint32_t));

                // Read attribute data
                std::vector<uint8_t> attrData(attrSize);
                file.read(reinterpret_cast<char*>(attrData.data()), attrSize);

                // Parse important attributes
                if (attrName == "dataWindow" && attrType == "box2i")
                {
                    std::memcpy(&dataWindowMinX, &attrData[0], 4);
                    std::memcpy(&dataWindowMinY, &attrData[4], 4);
                    std::memcpy(&dataWindowMaxX, &attrData[8], 4);
                    std::memcpy(&dataWindowMaxY, &attrData[12], 4);
                    width = dataWindowMaxX - dataWindowMinX + 1;
                    height = dataWindowMaxY - dataWindowMinY + 1;
                }
                else if (attrName == "compression" && attrType == "compression")
                {
                    compression = static_cast<EXRCompression>(attrData[0]);
                }
                else if (attrName == "channels" && attrType == "chlist")
                {
                    // Parse channel list (simplified)
                    size_t offset = 0;
                    while (offset < attrData.size())
                    {
                        std::string channelName;
                        while (offset < attrData.size() && attrData[offset] != '\0')
                        {
                            channelName += static_cast<char>(attrData[offset++]);
                        }
                        offset++;  // Skip null terminator

                        if (channelName.empty()) break;

                        channelNames.push_back(channelName);

                        // Read pixel type (4 bytes)
                        if (offset + 4 <= attrData.size())
                        {
                            uint32_t pixelType;
                            std::memcpy(&pixelType, &attrData[offset], 4);
                            channelTypes.push_back(static_cast<EXRPixelType>(pixelType));
                            offset += 4;
                        }

                        // Skip pLinear (1 byte) and reserved (3 bytes)
                        offset += 4;

                        // Skip xSampling and ySampling (4 bytes each)
                        offset += 8;
                    }
                }
            }

            if (width <= 0 || height <= 0)
            {
                throw std::runtime_error("Invalid EXR dimensions");
            }

            if (compression != EXRCompression::NONE)
            {
                throw std::runtime_error(
                    "Compressed EXR files not yet supported. "
                    "Use libraries like OpenEXR or tinyexr for full support");
            }

            // For uncompressed, read scanline data
            // This is a simplified implementation
            size_t numChannels = std::min(size_t(4), channelNames.size());
            size_t dataSize = width * height * 4;  // Always convert to RGBA

            auto data = std::make_unique<uint8_t[]>(dataSize);
            std::memset(data.get(), 255, dataSize);  // Default alpha to 255

            // Read scanline offset table
            std::vector<uint64_t> scanlineOffsets(height);
            file.read(reinterpret_cast<char*>(scanlineOffsets.data()), height * sizeof(uint64_t));

            // Read pixel data (simplified - assumes float32 RGB/RGBA)
            for (int y = 0; y < height; ++y)
            {
                // Read scanline header
                int32_t lineNumber;
                uint32_t dataSize;
                file.read(reinterpret_cast<char*>(&lineNumber), sizeof(int32_t));
                file.read(reinterpret_cast<char*>(&dataSize), sizeof(uint32_t));

                // Read scanline data
                std::vector<float> scanline(width * numChannels);
                file.read(reinterpret_cast<char*>(scanline.data()),
                          width * numChannels * sizeof(float));

                // Convert float to uint8 and store as RGBA
                for (int x = 0; x < width; ++x)
                {
                    size_t outIdx = (y * width + x) * 4;
                    for (size_t c = 0; c < numChannels && c < 3; ++c)
                    {
                        float val = scanline[x * numChannels + c];
                        val = std::clamp(val, 0.0f, 1.0f);
                        data[outIdx + c] = static_cast<uint8_t>(val * 255.0f);
                    }
                    // Alpha channel
                    if (numChannels >= 4)
                    {
                        float val = scanline[x * numChannels + 3];
                        val = std::clamp(val, 0.0f, 1.0f);
                        data[outIdx + 3] = static_cast<uint8_t>(val * 255.0f);
                    }
                }
            }

            bitmap.SetData(std::move(data));
            bitmap.SetSize(Vector2Uint(width, height));
            bitmap.SetBytesPerPixel(4);
            bitmap.SetFilename(filename);
        }

        static void Write(const Bitmap& bitmap, const std::filesystem::path& filename,
                          EXRCompression compression = EXRCompression::NONE)
        {
            if (!bitmap.GetData())
            {
                throw std::runtime_error("Cannot write empty bitmap");
            }

            const auto& size = bitmap.GetSize();
            int width = static_cast<int>(size.x);
            int height = static_cast<int>(size.y);
            uint32_t bytesPerPixel = bitmap.GetBytesPerPixel();

            if (compression != EXRCompression::NONE)
            {
                throw std::runtime_error(
                    "EXR compression not yet implemented. "
                    "Use NONE or integrate OpenEXR/tinyexr library");
            }

            std::ofstream file(filename, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to create EXR file: " + filename.string());
            }

            // Write header
            EXRHeader header;
            header.magic = EXR_MAGIC;
            header.version = EXR_VERSION;
            file.write(reinterpret_cast<const char*>(&header), sizeof(EXRHeader));

            // Helper to write string attribute
            auto writeStringAttr =
                [&](const std::string& name, const std::string& type, const std::string& value)
            {
                file.write(name.c_str(), name.length() + 1);
                file.write(type.c_str(), type.length() + 1);
                uint32_t size = value.length() + 1;
                file.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
                file.write(value.c_str(), size);
            };

            // Helper to write binary attribute
            auto writeBinaryAttr = [&](const std::string& name, const std::string& type,
                                       const void* data, uint32_t size)
            {
                file.write(name.c_str(), name.length() + 1);
                file.write(type.c_str(), type.length() + 1);
                file.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
                file.write(reinterpret_cast<const char*>(data), size);
            };

            // Write channels attribute
            {
                std::vector<uint8_t> channelData;
                const char* channels[] = {"B", "G", "R", "A"};

                for (int i = 0; i < 4; ++i)
                {
                    // Channel name
                    const char* ch = channels[i];
                    channelData.insert(channelData.end(), ch, ch + strlen(ch) + 1);

                    // Pixel type (FLOAT = 2)
                    uint32_t pixelType = 2;
                    uint8_t* pt = reinterpret_cast<uint8_t*>(&pixelType);
                    channelData.insert(channelData.end(), pt, pt + 4);

                    // pLinear (1) + reserved (3)
                    uint32_t linear = 0;
                    uint8_t* lin = reinterpret_cast<uint8_t*>(&linear);
                    channelData.insert(channelData.end(), lin, lin + 4);

                    // xSampling and ySampling
                    uint32_t sampling = 1;
                    uint8_t* samp = reinterpret_cast<uint8_t*>(&sampling);
                    channelData.insert(channelData.end(), samp, samp + 4);
                    channelData.insert(channelData.end(), samp, samp + 4);
                }
                channelData.push_back(0);  // Null terminator

                writeBinaryAttr("channels", "chlist", channelData.data(), channelData.size());
            }

            // Write compression
            {
                uint8_t comp = static_cast<uint8_t>(compression);
                writeBinaryAttr("compression", "compression", &comp, 1);
            }

            // Write dataWindow
            {
                int32_t box[4] = {0, 0, width - 1, height - 1};
                writeBinaryAttr("dataWindow", "box2i", box, sizeof(box));
            }

            // Write displayWindow
            {
                int32_t box[4] = {0, 0, width - 1, height - 1};
                writeBinaryAttr("displayWindow", "box2i", box, sizeof(box));
            }

            // Write lineOrder
            {
                uint8_t lineOrder = 0;  // Increasing Y
                writeBinaryAttr("lineOrder", "lineOrder", &lineOrder, 1);
            }

            // Write pixelAspectRatio
            {
                float aspectRatio = 1.0f;
                writeBinaryAttr("pixelAspectRatio", "float", &aspectRatio, sizeof(float));
            }

            // Write screenWindowCenter
            {
                float center[2] = {0.0f, 0.0f};
                writeBinaryAttr("screenWindowCenter", "v2f", center, sizeof(center));
            }

            // Write screenWindowWidth
            {
                float windowWidth = 1.0f;
                writeBinaryAttr("screenWindowWidth", "float", &windowWidth, sizeof(float));
            }

            // End of header
            file.put('\0');

            // Write scanline offset table (placeholder)
            std::vector<uint64_t> offsets(height, 0);
            uint64_t currentOffset = file.tellp();
            currentOffset += height * sizeof(uint64_t);

            for (int y = 0; y < height; ++y)
            {
                offsets[y] = currentOffset;
                uint32_t scanlineSize =
                    width * 4 * sizeof(float) + 8;  // +8 for line number and size
                currentOffset += scanlineSize;
            }

            file.write(reinterpret_cast<const char*>(offsets.data()), height * sizeof(uint64_t));

            // Write scanlines
            for (int y = 0; y < height; ++y)
            {
                int32_t lineNumber = y;
                uint32_t scanlineDataSize = width * 4 * sizeof(float);

                file.write(reinterpret_cast<const char*>(&lineNumber), sizeof(int32_t));
                file.write(reinterpret_cast<const char*>(&scanlineDataSize), sizeof(uint32_t));

                // Convert uint8 to float and write
                std::vector<float> scanline(width * 4);
                for (int x = 0; x < width; ++x)
                {
                    size_t srcIdx = (y * width + x) * bytesPerPixel;
                    size_t dstIdx = x * 4;

                    // BGRA order for EXR
                    scanline[dstIdx + 0] = bitmap.GetData()[srcIdx + 2] / 255.0f;  // B
                    scanline[dstIdx + 1] = bitmap.GetData()[srcIdx + 1] / 255.0f;  // G
                    scanline[dstIdx + 2] = bitmap.GetData()[srcIdx + 0] / 255.0f;  // R
                    scanline[dstIdx + 3] =
                        (bytesPerPixel >= 4) ? bitmap.GetData()[srcIdx + 3] / 255.0f : 1.0f;  // A
                }

                file.write(reinterpret_cast<const char*>(scanline.data()), scanlineDataSize);
            }

            if (!file)
            {
                throw std::runtime_error("Failed to write EXR data");
            }
        }

    private:
        static inline bool registered = Register("exr", "EXR");
    };
}