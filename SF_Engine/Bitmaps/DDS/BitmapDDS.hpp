#pragma once
#include <stb_image.h>
#include <Bitmaps/Bitmap.hpp>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace SF::Engine
{
    enum DDSFormat
    {
        ARGB8,
        XRGB8,
        A8L8,
        DXT1,
        DXT3,
        DXT5
    };

// DDS pixel format structure
#pragma pack(push, 1)
    struct DDSPixelFormat
    {
        uint32_t size;
        uint32_t flags;
        uint32_t fourCC;
        uint32_t rgbBitCount;
        uint32_t rBitMask;
        uint32_t gBitMask;
        uint32_t bBitMask;
        uint32_t aBitMask;
    };

    // DDS header structure
    struct DDSHeader
    {
        uint32_t magic;  // "DDS " (0x20534444)
        uint32_t size;   // Size of structure (124 bytes)
        uint32_t flags;
        uint32_t height;
        uint32_t width;
        uint32_t pitchOrLinearSize;
        uint32_t depth;
        uint32_t mipMapCount;
        uint32_t reserved1[11];
        DDSPixelFormat pixelFormat;
        uint32_t caps;
        uint32_t caps2;
        uint32_t caps3;
        uint32_t caps4;
        uint32_t reserved2;
    };
#pragma pack(pop)

    // DDS constants
    constexpr uint32_t DDS_MAGIC = 0x20534444;  // "DDS "
    constexpr uint32_t DDS_HEADER_SIZE = 124;

    // Flags
    constexpr uint32_t DDSD_CAPS = 0x1;
    constexpr uint32_t DDSD_HEIGHT = 0x2;
    constexpr uint32_t DDSD_WIDTH = 0x4;
    constexpr uint32_t DDSD_PITCH = 0x8;
    constexpr uint32_t DDSD_PIXELFORMAT = 0x1000;
    constexpr uint32_t DDSD_MIPMAPCOUNT = 0x20000;
    constexpr uint32_t DDSD_LINEARSIZE = 0x80000;

    // Pixel format flags
    constexpr uint32_t DDPF_ALPHAPIXELS = 0x1;
    constexpr uint32_t DDPF_ALPHA = 0x2;
    constexpr uint32_t DDPF_FOURCC = 0x4;
    constexpr uint32_t DDPF_RGB = 0x40;

    // FourCC codes
    constexpr uint32_t FOURCC_DXT1 = 0x31545844;  // "DXT1"
    constexpr uint32_t FOURCC_DXT3 = 0x33545844;  // "DXT3"
    constexpr uint32_t FOURCC_DXT5 = 0x35545844;  // "DXT5"

    // Caps
    constexpr uint32_t DDSCAPS_TEXTURE = 0x1000;

    class BitmapDDS : public Bitmap::Registrar<BitmapDDS>
    {
    public:
        static void Load(Bitmap& bitmap, const std::filesystem::path& filename)
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to open DDS file: " + filename.string());
            }

            // Read header
            DDSHeader header;
            file.read(reinterpret_cast<char*>(&header), sizeof(DDSHeader));

            if (header.magic != DDS_MAGIC)
            {
                throw std::runtime_error("Invalid DDS file magic number");
            }

            if (header.size != DDS_HEADER_SIZE)
            {
                throw std::runtime_error("Invalid DDS header size");
            }

            // Determine format and bytes per pixel
            uint32_t bytesPerPixel = 4;
            bool isCompressed = false;

            if (header.pixelFormat.flags & DDPF_FOURCC)
            {
                isCompressed = true;
                // Compressed formats will be decompressed to RGBA
            }
            else if (header.pixelFormat.flags & DDPF_RGB)
            {
                bytesPerPixel = header.pixelFormat.rgbBitCount / 8;
            }

            uint32_t width = header.width;
            uint32_t height = header.height;

            if (isCompressed)
            {
                // For compressed formats, we'll need to decompress
                // This is a simplified version - full DXT decompression is complex
                throw std::runtime_error(
                    "DXT compressed DDS loading not yet implemented. "
                    "Consider using a library like gli or DirectXTex");
            }

            // Read uncompressed pixel data
            size_t dataSize = width * height * bytesPerPixel;
            auto data = std::make_unique<uint8_t[]>(dataSize);
            file.read(reinterpret_cast<char*>(data.get()), dataSize);

            if (!file)
            {
                throw std::runtime_error("Failed to read DDS pixel data");
            }

            // Convert BGRA to RGBA if needed
            if (bytesPerPixel == 4)
            {
                for (size_t i = 0; i < dataSize; i += 4)
                {
                    std::swap(data[i], data[i + 2]);  // Swap B and R
                }
            }

            bitmap.SetData(std::move(data));
            bitmap.SetSize(Vector2Uint(width, height));
            bitmap.SetBytesPerPixel(bytesPerPixel);
            bitmap.SetFilename(filename);
        }

        static void Write(const Bitmap& bitmap, const std::filesystem::path& filename,
                          DDSFormat format = DDSFormat::ARGB8)
        {
            if (!bitmap.GetData())
            {
                throw std::runtime_error("Cannot write empty bitmap");
            }

            const auto& size = bitmap.GetSize();
            uint32_t width = size.x;
            uint32_t height = size.y;
            uint32_t bytesPerPixel = bitmap.GetBytesPerPixel();

            // Create header
            DDSHeader header = {};
            header.magic = DDS_MAGIC;
            header.size = DDS_HEADER_SIZE;
            header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH;
            header.width = width;
            header.height = height;
            header.depth = 1;
            header.mipMapCount = 1;
            header.caps = DDSCAPS_TEXTURE;

            // Setup pixel format based on requested format
            header.pixelFormat.size = sizeof(DDSPixelFormat);

            switch (format)
            {
                case DDSFormat::ARGB8:
                    header.pixelFormat.flags = DDPF_RGB | DDPF_ALPHAPIXELS;
                    header.pixelFormat.rgbBitCount = 32;
                    header.pixelFormat.rBitMask = 0x00FF0000;
                    header.pixelFormat.gBitMask = 0x0000FF00;
                    header.pixelFormat.bBitMask = 0x000000FF;
                    header.pixelFormat.aBitMask = 0xFF000000;
                    header.pitchOrLinearSize = width * 4;
                    break;

                case DDSFormat::XRGB8:
                    header.pixelFormat.flags = DDPF_RGB;
                    header.pixelFormat.rgbBitCount = 32;
                    header.pixelFormat.rBitMask = 0x00FF0000;
                    header.pixelFormat.gBitMask = 0x0000FF00;
                    header.pixelFormat.bBitMask = 0x000000FF;
                    header.pixelFormat.aBitMask = 0x00000000;
                    header.pitchOrLinearSize = width * 4;
                    break;

                case DDSFormat::DXT1:
                case DDSFormat::DXT3:
                case DDSFormat::DXT5:
                    throw std::runtime_error("DXT compression not yet implemented");

                default:
                    throw std::runtime_error("Unsupported DDS format");
            }

            // Write to file
            std::ofstream file(filename, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to create DDS file: " + filename.string());
            }

            // Write header
            file.write(reinterpret_cast<const char*>(&header), sizeof(DDSHeader));

            // Convert and write pixel data (RGBA to BGRA)
            size_t dataSize = width * height * bytesPerPixel;
            auto writeData = std::make_unique<uint8_t[]>(dataSize);
            std::memcpy(writeData.get(), bitmap.GetData().get(), dataSize);

            if (bytesPerPixel == 4)
            {
                for (size_t i = 0; i < dataSize; i += 4)
                {
                    std::swap(writeData[i], writeData[i + 2]);  // Swap R and B
                }
            }

            file.write(reinterpret_cast<const char*>(writeData.get()), dataSize);

            if (!file)
            {
                throw std::runtime_error("Failed to write DDS pixel data");
            }
        }

    private:
        static inline bool registered = Register("dds", "DDS");
    };
}