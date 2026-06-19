#pragma once
#include <stb_image.h>
#include <Bitmaps/Bitmap.hpp>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <algorithm>

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

    struct DDSHeader
    {
        uint32_t magic;
        uint32_t size;
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

    constexpr uint32_t DDS_MAGIC = 0x20534444;
    constexpr uint32_t DDS_HEADER_SIZE = 124;

    constexpr uint32_t DDSD_CAPS = 0x1;
    constexpr uint32_t DDSD_HEIGHT = 0x2;
    constexpr uint32_t DDSD_WIDTH = 0x4;
    constexpr uint32_t DDSD_PITCH = 0x8;
    constexpr uint32_t DDSD_PIXELFORMAT = 0x1000;
    constexpr uint32_t DDSD_MIPMAPCOUNT = 0x20000;
    constexpr uint32_t DDSD_LINEARSIZE = 0x80000;

    constexpr uint32_t DDPF_ALPHAPIXELS = 0x1;
    constexpr uint32_t DDPF_ALPHA = 0x2;
    constexpr uint32_t DDPF_FOURCC = 0x4;
    constexpr uint32_t DDPF_RGB = 0x40;
    constexpr uint32_t DDPF_LUMINANCE = 0x20000;

    constexpr uint32_t FOURCC_DXT1 = 0x31545844;
    constexpr uint32_t FOURCC_DXT3 = 0x33545844;
    constexpr uint32_t FOURCC_DXT5 = 0x35545844;

    constexpr uint32_t DDSCAPS_TEXTURE = 0x1000;

    class BitmapDDS : public Bitmap::Registrar<BitmapDDS>
    {
    public:
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename)
        {
            std::ifstream file(filename, std::ios::binary);
            if (!file)
                throw std::runtime_error("Failed to open DDS file: " + filename.string());

            DDSHeader header;
            file.read(reinterpret_cast<char *>(&header), sizeof(DDSHeader));

            if (header.magic != DDS_MAGIC)
                throw std::runtime_error("Invalid DDS magic number");
            if (header.size != DDS_HEADER_SIZE)
                throw std::runtime_error("Invalid DDS header size");

            const uint32_t width = header.width;
            const uint32_t height = header.height;
            const auto &pf = header.pixelFormat;

            if (pf.flags & DDPF_FOURCC)
            {
                uint32_t blockSize = (pf.fourCC == FOURCC_DXT1) ? 8 : 16;
                uint32_t blocksW = std::max(1u, (width + 3) / 4);
                uint32_t blocksH = std::max(1u, (height + 3) / 4);
                size_t compressed = blocksW * blocksH * blockSize;

                std::vector<uint8_t> src(compressed);
                file.read(reinterpret_cast<char *>(src.data()), compressed);
                if (!file)
                    throw std::runtime_error("Failed to read DXT data");

                auto rgba = std::make_unique<uint8_t[]>(width * height * 4);

                if (pf.fourCC == FOURCC_DXT1)
                    DecompressDXT1(src, rgba.get(), width, height);
                else if (pf.fourCC == FOURCC_DXT3)
                    DecompressDXT3(src, rgba.get(), width, height);
                else if (pf.fourCC == FOURCC_DXT5)
                    DecompressDXT5(src, rgba.get(), width, height);
                else
                    throw std::runtime_error("Unknown FourCC format");

                bitmap.SetData(std::move(rgba));
                bitmap.SetSize(Vector2Uint(width, height));
                bitmap.SetBytesPerPixel(4);
                bitmap.SetFilename(filename);
                return;
            }

            if ((pf.flags & (DDPF_ALPHA | DDPF_LUMINANCE)) && pf.rgbBitCount == 16)
            {
                size_t pixels = width * height;
                std::vector<uint8_t> raw(pixels * 2);
                file.read(reinterpret_cast<char *>(raw.data()), pixels * 2);
                if (!file)
                    throw std::runtime_error("Failed to read A8L8 data");

                auto rgba = std::make_unique<uint8_t[]>(pixels * 4);
                for (size_t i = 0; i < pixels; ++i)
                {
                    uint8_t l = raw[i * 2 + 0];
                    uint8_t a = raw[i * 2 + 1];
                    rgba[i * 4 + 0] = l;
                    rgba[i * 4 + 1] = l;
                    rgba[i * 4 + 2] = l;
                    rgba[i * 4 + 3] = a;
                }

                bitmap.SetData(std::move(rgba));
                bitmap.SetSize(Vector2Uint(width, height));
                bitmap.SetBytesPerPixel(4);
                bitmap.SetFilename(filename);
                return;
            }
            {
                uint32_t bytesPerPixel = pf.rgbBitCount / 8;
                size_t dataSize = width * height * bytesPerPixel;
                auto data = std::make_unique<uint8_t[]>(dataSize);

                file.read(reinterpret_cast<char *>(data.get()), dataSize);
                if (!file)
                    throw std::runtime_error("Failed to read pixel data");

                // BGRA → RGBA
                if (bytesPerPixel >= 3)
                {
                    for (size_t i = 0; i < dataSize; i += bytesPerPixel)
                        std::swap(data[i], data[i + 2]);
                }

                bitmap.SetData(std::move(data));
                bitmap.SetSize(Vector2Uint(width, height));
                bitmap.SetBytesPerPixel(bytesPerPixel);
                bitmap.SetFilename(filename);
            }
        }

        static void Write(const Bitmap &bitmap, const std::filesystem::path &filename,
                          DDSFormat format = DDSFormat::ARGB8)
        {
            if (!bitmap.GetData())
                throw std::runtime_error("Cannot write empty bitmap");

            const uint32_t width = bitmap.GetSize().x;
            const uint32_t height = bitmap.GetSize().y;
            const uint32_t srcBpp = bitmap.GetBytesPerPixel();

            DDSHeader header = {};
            header.magic = DDS_MAGIC;
            header.size = DDS_HEADER_SIZE;
            header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH;
            header.width = width;
            header.height = height;
            header.depth = 1;
            header.mipMapCount = 1;
            header.caps = DDSCAPS_TEXTURE;
            header.pixelFormat.size = sizeof(DDSPixelFormat);

            uint32_t writeBpp = srcBpp;

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
                writeBpp = 4;
                break;

            case DDSFormat::XRGB8:
                header.pixelFormat.flags = DDPF_RGB;
                header.pixelFormat.rgbBitCount = 32;
                header.pixelFormat.rBitMask = 0x00FF0000;
                header.pixelFormat.gBitMask = 0x0000FF00;
                header.pixelFormat.bBitMask = 0x000000FF;
                header.pixelFormat.aBitMask = 0x00000000;
                header.pitchOrLinearSize = width * 4;
                writeBpp = 4;
                break;

            case DDSFormat::A8L8:
                header.pixelFormat.flags = DDPF_LUMINANCE | DDPF_ALPHAPIXELS;
                header.pixelFormat.rgbBitCount = 16;
                header.pixelFormat.rBitMask = 0x000000FF; // luminance
                header.pixelFormat.aBitMask = 0x0000FF00; // alpha
                header.pitchOrLinearSize = width * 2;
                writeBpp = 2;
                break;

            case DDSFormat::DXT1:
                header.pixelFormat.flags = DDPF_FOURCC;
                header.pixelFormat.fourCC = FOURCC_DXT1;
                header.flags = (header.flags & ~DDSD_PITCH) | DDSD_LINEARSIZE;
                header.pitchOrLinearSize = std::max(1u, (width + 3) / 4) * std::max(1u, (height + 3) / 4) * 8;
                writeBpp = 0;
                break;

            case DDSFormat::DXT3:
                header.pixelFormat.flags = DDPF_FOURCC;
                header.pixelFormat.fourCC = FOURCC_DXT3;
                header.flags = (header.flags & ~DDSD_PITCH) | DDSD_LINEARSIZE;
                header.pitchOrLinearSize = std::max(1u, (width + 3) / 4) * std::max(1u, (height + 3) / 4) * 16;
                writeBpp = 0;
                break;

            case DDSFormat::DXT5:
                header.pixelFormat.flags = DDPF_FOURCC;
                header.pixelFormat.fourCC = FOURCC_DXT5;
                header.flags = (header.flags & ~DDSD_PITCH) | DDSD_LINEARSIZE;
                header.pitchOrLinearSize = std::max(1u, (width + 3) / 4) * std::max(1u, (height + 3) / 4) * 16;
                writeBpp = 0;
                break;

            default:
                throw std::runtime_error("Unsupported DDS format");
            }

            if (writeBpp == 0)
                throw std::runtime_error("DXT compression not yet implemented :( "
                                         "link against DirectXTex or squish and call their encoder here");

            std::ofstream out(filename, std::ios::binary);
            if (!out)
                throw std::runtime_error("Failed to create DDS file: " + filename.string());

            out.write(reinterpret_cast<const char *>(&header), sizeof(DDSHeader));

            const uint8_t *src = bitmap.GetData().get();
            size_t pixels = width * height;

            if (format == DDSFormat::A8L8)
            {
                // RGBA8 → A8L8
                auto buf = std::make_unique<uint8_t[]>(pixels * 2);
                for (size_t i = 0; i < pixels; ++i)
                {
                    uint8_t r = src[i * srcBpp + 0];
                    uint8_t g = src[i * srcBpp + 1];
                    uint8_t b = src[i * srcBpp + 2];
                    uint8_t a = (srcBpp == 4) ? src[i * srcBpp + 3] : 0xFF;
                    uint8_t l = static_cast<uint8_t>(0.299f * r + 0.587f * g + 0.114f * b);
                    buf[i * 2 + 0] = l;
                    buf[i * 2 + 1] = a;
                }
                out.write(reinterpret_cast<const char *>(buf.get()), pixels * 2);
            }
            else
            {
                // RGBA → BGRA for ARGB8 / XRGB8
                size_t dataSize = pixels * writeBpp;
                auto buf = std::make_unique<uint8_t[]>(dataSize);
                std::memcpy(buf.get(), src, dataSize);

                if (writeBpp >= 3)
                {
                    for (size_t i = 0; i < dataSize; i += writeBpp)
                        std::swap(buf[i], buf[i + 2]);
                }

                out.write(reinterpret_cast<const char *>(buf.get()), dataSize);
            }

            if (!out)
                throw std::runtime_error("Failed to write DDS pixel data");
        }

    private:
        static inline bool registered = Register("dds", "DDS");

        // Expand a packed RGB565 word into R8 G8 B8.
        static void Unpack565(uint16_t packed, uint8_t &r, uint8_t &g, uint8_t &b)
        {
            r = static_cast<uint8_t>(((packed >> 11) & 0x1F) * 255 / 31);
            g = static_cast<uint8_t>(((packed >> 5) & 0x3F) * 255 / 63);
            b = static_cast<uint8_t>(((packed >> 0) & 0x1F) * 255 / 31);
        }

        // Decode one 4×4 DXT colour block into `out` (stride = imageWidth * 4).
        // If isDXT1 and the colour0 <= colour1 code point 3 is transparent.
        static void DecodeColourBlock(const uint8_t *block,
                                      uint8_t *out,
                                      uint32_t imageWidth,
                                      bool isDXT1)
        {
            uint16_t c0 = static_cast<uint16_t>(block[0] | (block[1] << 8));
            uint16_t c1 = static_cast<uint16_t>(block[2] | (block[3] << 8));

            uint8_t r[4], g[4], b[4], a[4];
            Unpack565(c0, r[0], g[0], b[0]);
            Unpack565(c1, r[1], g[1], b[1]);

            a[0] = a[1] = a[2] = a[3] = 0xFF;

            if (!isDXT1 || c0 > c1)
            {
                // 4-colour mode
                r[2] = (2 * r[0] + r[1]) / 3;
                g[2] = (2 * g[0] + g[1]) / 3;
                b[2] = (2 * b[0] + b[1]) / 3;
                r[3] = (r[0] + 2 * r[1]) / 3;
                g[3] = (g[0] + 2 * g[1]) / 3;
                b[3] = (b[0] + 2 * b[1]) / 3;
            }
            else
            {
                // 3-colour + transparent mode (DXT1 only)
                r[2] = (r[0] + r[1]) / 2;
                g[2] = (g[0] + g[1]) / 2;
                b[2] = (b[0] + b[1]) / 2;
                r[3] = g[3] = b[3] = 0;
                a[3] = 0;
            }

            uint32_t indices = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);

            for (int row = 0; row < 4; ++row)
            {
                for (int col = 0; col < 4; ++col)
                {
                    uint8_t idx = (indices >> ((row * 4 + col) * 2)) & 0x3;
                    uint8_t *px = out + (row * imageWidth + col) * 4;
                    px[0] = r[idx];
                    px[1] = g[idx];
                    px[2] = b[idx];
                    px[3] = a[idx];
                }
            }
        }

        // DXT3 explicit 4-bit alpha block.
        static void DecodeDXT3AlphaBlock(const uint8_t *block,
                                         uint8_t *out,
                                         uint32_t imageWidth)
        {
            for (int row = 0; row < 4; ++row)
            {
                uint16_t word = static_cast<uint16_t>(block[row * 2] | (block[row * 2 + 1] << 8));
                for (int col = 0; col < 4; ++col)
                {
                    uint8_t a = (word >> (col * 4)) & 0xF;
                    out[(row * imageWidth + col) * 4 + 3] = static_cast<uint8_t>(a * 17); // 0xF→0xFF
                }
            }
        }

        // DXT5 interpolated alpha block.
        static void DecodeDXT5AlphaBlock(const uint8_t *block,
                                         uint8_t *out,
                                         uint32_t imageWidth)
        {
            uint8_t a[8];
            a[0] = block[0];
            a[1] = block[1];

            if (a[0] > a[1])
            {
                a[2] = (6 * a[0] + 1 * a[1]) / 7;
                a[3] = (5 * a[0] + 2 * a[1]) / 7;
                a[4] = (4 * a[0] + 3 * a[1]) / 7;
                a[5] = (3 * a[0] + 4 * a[1]) / 7;
                a[6] = (2 * a[0] + 5 * a[1]) / 7;
                a[7] = (1 * a[0] + 6 * a[1]) / 7;
            }
            else
            {
                a[2] = (4 * a[0] + 1 * a[1]) / 5;
                a[3] = (3 * a[0] + 2 * a[1]) / 5;
                a[4] = (2 * a[0] + 3 * a[1]) / 5;
                a[5] = (1 * a[0] + 4 * a[1]) / 5;
                a[6] = 0;
                a[7] = 255;
            }

            // 48-bit index table packed in 6 bytes starting at block[2]
            uint64_t bits = 0;
            for (int i = 0; i < 6; ++i)
                bits |= static_cast<uint64_t>(block[2 + i]) << (i * 8);

            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 4; ++col)
                {
                    uint8_t idx = (bits >> ((row * 4 + col) * 3)) & 0x7;
                    out[(row * imageWidth + col) * 4 + 3] = a[idx];
                }
        }

        static void DecompressDXT1(const std::vector<uint8_t> &src,
                                   uint8_t *dst,
                                   uint32_t width, uint32_t height)
        {
            uint32_t blocksW = std::max(1u, (width + 3) / 4);
            uint32_t blocksH = std::max(1u, (height + 3) / 4);
            const uint8_t *p = src.data();

            for (uint32_t by = 0; by < blocksH; ++by)
                for (uint32_t bx = 0; bx < blocksW; ++bx, p += 8)
                {
                    uint8_t *out = dst + (by * 4 * width + bx * 4) * 4;
                    DecodeColourBlock(p, out, width, true);
                }
        }

        static void DecompressDXT3(const std::vector<uint8_t> &src,
                                   uint8_t *dst,
                                   uint32_t width, uint32_t height)
        {
            uint32_t blocksW = std::max(1u, (width + 3) / 4);
            uint32_t blocksH = std::max(1u, (height + 3) / 4);
            const uint8_t *p = src.data();

            for (uint32_t by = 0; by < blocksH; ++by)
                for (uint32_t bx = 0; bx < blocksW; ++bx, p += 16)
                {
                    uint8_t *out = dst + (by * 4 * width + bx * 4) * 4;
                    DecodeDXT3AlphaBlock(p, out, width);         // first 8 bytes = alpha
                    DecodeColourBlock(p + 8, out, width, false); // next 8 = colour
                }
        }

        static void DecompressDXT5(const std::vector<uint8_t> &src,
                                   uint8_t *dst,
                                   uint32_t width, uint32_t height)
        {
            uint32_t blocksW = std::max(1u, (width + 3) / 4);
            uint32_t blocksH = std::max(1u, (height + 3) / 4);
            const uint8_t *p = src.data();

            for (uint32_t by = 0; by < blocksH; ++by)
                for (uint32_t bx = 0; bx < blocksW; ++bx, p += 16)
                {
                    uint8_t *out = dst + (by * 4 * width + bx * 4) * 4;
                    DecodeDXT5AlphaBlock(p, out, width);         // first 8 bytes = alpha
                    DecodeColourBlock(p + 8, out, width, false); // next 8 = colour
                }
        }
    };
}