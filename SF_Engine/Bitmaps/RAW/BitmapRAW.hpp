#pragma once
#include <Bitmaps/Bitmap.hpp>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace SF::Engine
{
    // RAW image format specification
    enum class RAWPixelFormat
    {
        R8,      // 8-bit grayscale
        RG8,     // 8-bit, 2 channels
        RGB8,    // 8-bit RGB
        RGBA8,   // 8-bit RGBA
        R16,     // 16-bit grayscale
        RG16,    // 16-bit, 2 channels
        RGB16,   // 16-bit RGB
        RGBA16,  // 16-bit RGBA
        R32F,    // 32-bit float grayscale
        RG32F,   // 32-bit float, 2 channels
        RGB32F,  // 32-bit float RGB
        RGBA32F  // 32-bit float RGBA
    };

    enum class RAWByteOrder
    {
        LITTLE_ENDIAN,
        BIG_ENDIAN
    };

#pragma pack(push, 1)
    // Optional RAW header (custom format)
    struct RAWHeader
    {
        char magic[4];  // "SRAW" (SF Raw)
        uint32_t width;
        uint32_t height;
        uint8_t pixelFormat;  // RAWPixelFormat enum value
        uint8_t byteOrder;    // RAWByteOrder enum value
        uint16_t reserved;
    };
#pragma pack(pop)

    constexpr char RAW_MAGIC[4] = {'S', 'R', 'A', 'W'};
    constexpr size_t RAW_HEADER_SIZE = sizeof(RAWHeader);

    class BitmapRAW : public Bitmap::Registrar<BitmapRAW>
    {
    public:
        // Configuration for headerless RAW files
        struct RAWConfig
        {
            uint32_t width = 0;
            uint32_t height = 0;
            RAWPixelFormat format = RAWPixelFormat::RGB8;
            RAWByteOrder byteOrder = RAWByteOrder::LITTLE_ENDIAN;
            bool hasHeader = false;
        };

        static void Load(Bitmap& bitmap, const std::filesystem::path& filename)
        {
            // Try loading with auto-detection
            RAWConfig config;
            config.hasHeader = true;
            LoadWithConfig(bitmap, filename, config);
        }

        static void LoadWithConfig(Bitmap& bitmap, const std::filesystem::path& filename,
                                   const RAWConfig& config)
        {
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file)
            {
                throw std::runtime_error("Failed to open RAW file: " + filename.string());
            }

            size_t fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            uint32_t width = config.width;
            uint32_t height = config.height;
            RAWPixelFormat format = config.format;
            RAWByteOrder byteOrder = config.byteOrder;
            size_t dataOffset = 0;

            // Check for header
            if (config.hasHeader)
            {
                RAWHeader header;
                file.read(reinterpret_cast<char*>(&header), sizeof(RAWHeader));

                if (std::memcmp(header.magic, RAW_MAGIC, 4) == 0)
                {
                    width = header.width;
                    height = header.height;
                    format = static_cast<RAWPixelFormat>(header.pixelFormat);
                    byteOrder = static_cast<RAWByteOrder>(header.byteOrder);
                    dataOffset = RAW_HEADER_SIZE;
                }
                else
                {
                    // No header found, use config parameters
                    file.seekg(0, std::ios::beg);
                    dataOffset = 0;
                }
            }

            if (width == 0 || height == 0)
            {
                throw std::runtime_error(
                    "RAW dimensions not specified. Use LoadWithConfig() "
                    "to specify width, height, and format");
            }

            // Get bytes per pixel from format
            size_t bytesPerPixel = GetBytesPerPixel(format);
            size_t expectedDataSize = width * height * bytesPerPixel;

            if (fileSize - dataOffset < expectedDataSize)
            {
                throw std::runtime_error("RAW file size mismatch. Expected " +
                                         std::to_string(expectedDataSize) + " bytes, got " +
                                         std::to_string(fileSize - dataOffset));
            }

            // Read raw pixel data
            std::vector<uint8_t> rawData(expectedDataSize);
            file.read(reinterpret_cast<char*>(rawData.data()), expectedDataSize);

            // Convert to RGBA8 for internal storage
            size_t outputSize = width * height * 4;
            auto data = std::make_unique<uint8_t[]>(outputSize);

            ConvertToRGBA8(rawData.data(), data.get(), width, height, format, byteOrder);

            bitmap.SetData(std::move(data));
            bitmap.SetSize(UVec2(width, height));
            bitmap.SetBytesPerPixel(4);
            bitmap.SetFilename(filename);
        }

        static void Write(const Bitmap& bitmap, const std::filesystem::path& filename,
                          RAWPixelFormat format = RAWPixelFormat::RGBA8, bool includeHeader = true)
        {
            if (!bitmap.GetData())
            {
                throw std::runtime_error("Cannot write empty bitmap");
            }

            const auto& size = bitmap.GetSize();
            uint32_t width = size.x;
            uint32_t height = size.y;

            std::ofstream file(filename, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to create RAW file: " + filename.string());
            }

            // Write header if requested
            if (includeHeader)
            {
                RAWHeader header = {};
                std::memcpy(header.magic, RAW_MAGIC, 4);
                header.width = width;
                header.height = height;
                header.pixelFormat = static_cast<uint8_t>(format);
                header.byteOrder = static_cast<uint8_t>(RAWByteOrder::LITTLE_ENDIAN);
                header.reserved = 0;

                file.write(reinterpret_cast<const char*>(&header), sizeof(RAWHeader));
            }

            // Convert RGBA8 to requested format
            size_t outputBytesPerPixel = GetBytesPerPixel(format);
            size_t outputSize = width * height * outputBytesPerPixel;
            std::vector<uint8_t> outputData(outputSize);

            ConvertFromRGBA8(bitmap.GetData().get(), outputData.data(), width, height, format);

            file.write(reinterpret_cast<const char*>(outputData.data()), outputSize);

            if (!file)
            {
                throw std::runtime_error("Failed to write RAW pixel data");
            }
        }

    private:
        static size_t GetBytesPerPixel(RAWPixelFormat format)
        {
            switch (format)
            {
                case RAWPixelFormat::R8:
                    return 1;
                case RAWPixelFormat::RG8:
                    return 2;
                case RAWPixelFormat::RGB8:
                    return 3;
                case RAWPixelFormat::RGBA8:
                    return 4;
                case RAWPixelFormat::R16:
                    return 2;
                case RAWPixelFormat::RG16:
                    return 4;
                case RAWPixelFormat::RGB16:
                    return 6;
                case RAWPixelFormat::RGBA16:
                    return 8;
                case RAWPixelFormat::R32F:
                    return 4;
                case RAWPixelFormat::RG32F:
                    return 8;
                case RAWPixelFormat::RGB32F:
                    return 12;
                case RAWPixelFormat::RGBA32F:
                    return 16;
                default:
                    return 4;
            }
        }

        static void ConvertToRGBA8(const uint8_t* src, uint8_t* dst, uint32_t width,
                                   uint32_t height, RAWPixelFormat format, RAWByteOrder byteOrder)
        {
            size_t pixelCount = width * height;

            switch (format)
            {
                case RAWPixelFormat::R8:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        dst[i * 4 + 0] = src[i];
                        dst[i * 4 + 1] = src[i];
                        dst[i * 4 + 2] = src[i];
                        dst[i * 4 + 3] = 255;
                    }
                    break;

                case RAWPixelFormat::RG8:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        dst[i * 4 + 0] = src[i * 2 + 0];
                        dst[i * 4 + 1] = src[i * 2 + 1];
                        dst[i * 4 + 2] = 0;
                        dst[i * 4 + 3] = 255;
                    }
                    break;

                case RAWPixelFormat::RGB8:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        dst[i * 4 + 0] = src[i * 3 + 0];
                        dst[i * 4 + 1] = src[i * 3 + 1];
                        dst[i * 4 + 2] = src[i * 3 + 2];
                        dst[i * 4 + 3] = 255;
                    }
                    break;

                case RAWPixelFormat::RGBA8:
                    std::memcpy(dst, src, pixelCount * 4);
                    break;

                case RAWPixelFormat::R16:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        uint16_t val = *reinterpret_cast<const uint16_t*>(&src[i * 2]);
                        uint8_t converted = static_cast<uint8_t>(val >> 8);
                        dst[i * 4 + 0] = converted;
                        dst[i * 4 + 1] = converted;
                        dst[i * 4 + 2] = converted;
                        dst[i * 4 + 3] = 255;
                    }
                    break;

                case RAWPixelFormat::RGB16:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        const uint16_t* pixel16 = reinterpret_cast<const uint16_t*>(&src[i * 6]);
                        dst[i * 4 + 0] = static_cast<uint8_t>(pixel16[0] >> 8);
                        dst[i * 4 + 1] = static_cast<uint8_t>(pixel16[1] >> 8);
                        dst[i * 4 + 2] = static_cast<uint8_t>(pixel16[2] >> 8);
                        dst[i * 4 + 3] = 255;
                    }
                    break;

                case RAWPixelFormat::RGBA16:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        const uint16_t* pixel16 = reinterpret_cast<const uint16_t*>(&src[i * 8]);
                        dst[i * 4 + 0] = static_cast<uint8_t>(pixel16[0] >> 8);
                        dst[i * 4 + 1] = static_cast<uint8_t>(pixel16[1] >> 8);
                        dst[i * 4 + 2] = static_cast<uint8_t>(pixel16[2] >> 8);
                        dst[i * 4 + 3] = static_cast<uint8_t>(pixel16[3] >> 8);
                    }
                    break;

                case RAWPixelFormat::R32F:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        float val = *reinterpret_cast<const float*>(&src[i * 4]);
                        val = std::clamp(val, 0.0f, 1.0f);
                        uint8_t converted = static_cast<uint8_t>(val * 255.0f);
                        dst[i * 4 + 0] = converted;
                        dst[i * 4 + 1] = converted;
                        dst[i * 4 + 2] = converted;
                        dst[i * 4 + 3] = 255;
                    }
                    break;

                case RAWPixelFormat::RGB32F:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        const float* pixel32 = reinterpret_cast<const float*>(&src[i * 12]);
                        dst[i * 4 + 0] =
                            static_cast<uint8_t>(std::clamp(pixel32[0], 0.0f, 1.0f) * 255.0f);
                        dst[i * 4 + 1] =
                            static_cast<uint8_t>(std::clamp(pixel32[1], 0.0f, 1.0f) * 255.0f);
                        dst[i * 4 + 2] =
                            static_cast<uint8_t>(std::clamp(pixel32[2], 0.0f, 1.0f) * 255.0f);
                        dst[i * 4 + 3] = 255;
                    }
                    break;

                case RAWPixelFormat::RGBA32F:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        const float* pixel32 = reinterpret_cast<const float*>(&src[i * 16]);
                        dst[i * 4 + 0] =
                            static_cast<uint8_t>(std::clamp(pixel32[0], 0.0f, 1.0f) * 255.0f);
                        dst[i * 4 + 1] =
                            static_cast<uint8_t>(std::clamp(pixel32[1], 0.0f, 1.0f) * 255.0f);
                        dst[i * 4 + 2] =
                            static_cast<uint8_t>(std::clamp(pixel32[2], 0.0f, 1.0f) * 255.0f);
                        dst[i * 4 + 3] =
                            static_cast<uint8_t>(std::clamp(pixel32[3], 0.0f, 1.0f) * 255.0f);
                    }
                    break;
            }
        }

        static void ConvertFromRGBA8(const uint8_t* src, uint8_t* dst, uint32_t width,
                                     uint32_t height, RAWPixelFormat format)
        {
            size_t pixelCount = width * height;

            switch (format)
            {
                case RAWPixelFormat::R8:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        // Convert to grayscale
                        dst[i] =
                            static_cast<uint8_t>(0.299f * src[i * 4 + 0] + 0.587f * src[i * 4 + 1] +
                                                 0.114f * src[i * 4 + 2]);
                    }
                    break;

                case RAWPixelFormat::RG8:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        dst[i * 2 + 0] = src[i * 4 + 0];
                        dst[i * 2 + 1] = src[i * 4 + 1];
                    }
                    break;

                case RAWPixelFormat::RGB8:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        dst[i * 3 + 0] = src[i * 4 + 0];
                        dst[i * 3 + 1] = src[i * 4 + 1];
                        dst[i * 3 + 2] = src[i * 4 + 2];
                    }
                    break;

                case RAWPixelFormat::RGBA8:
                    std::memcpy(dst, src, pixelCount * 4);
                    break;

                case RAWPixelFormat::RGB16:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        uint16_t* pixel16 = reinterpret_cast<uint16_t*>(&dst[i * 6]);
                        pixel16[0] = static_cast<uint16_t>(src[i * 4 + 0]) << 8;
                        pixel16[1] = static_cast<uint16_t>(src[i * 4 + 1]) << 8;
                        pixel16[2] = static_cast<uint16_t>(src[i * 4 + 2]) << 8;
                    }
                    break;

                case RAWPixelFormat::RGBA16:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        uint16_t* pixel16 = reinterpret_cast<uint16_t*>(&dst[i * 8]);
                        pixel16[0] = static_cast<uint16_t>(src[i * 4 + 0]) << 8;
                        pixel16[1] = static_cast<uint16_t>(src[i * 4 + 1]) << 8;
                        pixel16[2] = static_cast<uint16_t>(src[i * 4 + 2]) << 8;
                        pixel16[3] = static_cast<uint16_t>(src[i * 4 + 3]) << 8;
                    }
                    break;

                case RAWPixelFormat::RGB32F:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        float* pixel32 = reinterpret_cast<float*>(&dst[i * 12]);
                        pixel32[0] = src[i * 4 + 0] / 255.0f;
                        pixel32[1] = src[i * 4 + 1] / 255.0f;
                        pixel32[2] = src[i * 4 + 2] / 255.0f;
                    }
                    break;

                case RAWPixelFormat::RGBA32F:
                    for (size_t i = 0; i < pixelCount; ++i)
                    {
                        float* pixel32 = reinterpret_cast<float*>(&dst[i * 16]);
                        pixel32[0] = src[i * 4 + 0] / 255.0f;
                        pixel32[1] = src[i * 4 + 1] / 255.0f;
                        pixel32[2] = src[i * 4 + 2] / 255.0f;
                        pixel32[3] = src[i * 4 + 3] / 255.0f;
                    }
                    break;

                default:
                    break;
            }
        }

        static inline bool registered = Register("raw", "RAW", "data", "DATA");
    };
}