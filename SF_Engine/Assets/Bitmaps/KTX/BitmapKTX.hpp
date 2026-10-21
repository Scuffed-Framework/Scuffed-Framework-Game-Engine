#pragma once
#include <Assets/Bitmaps/Bitmap.hpp>

// ktxparse 0.1.0 (https://github.com/sevmeyer/ktxparse). Adjust the include path to wherever the header lives.
#include "KTXParse.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <UtilityClasses/ConstantExpression.hpp>

namespace SF::Engine
{
    // KTX 1.x (OpenGL-style) reader/writer for UNCOMPRESSED 2D textures.
    //
    // Load:  parses with ktxparse and returns mip 0 of the first array layer / cubemap face in its native
    //        channel type (8/16-bit, half or float, 1-4 channels). BGR/BGRA is swizzled to RGB/RGBA.
    //        Compressed KTX (BCn/ETC/ASTC), 3D textures and KTX2 files are rejected with a clear error.
    // Write: single-level uncompressed KTX1 for any Bitmap pixel format.
    class BitmapKTX : public Bitmap::Registrar<BitmapKTX>
    {
        static_assert(std::endian::native == std::endian::little, "BitmapKTX writer assumes a little-endian host");

    public:
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename)
        {
            LoadWithOptions(bitmap, filename, false);
        }

        static void LoadWithOptions(Bitmap &bitmap, const std::filesystem::path &filename, bool flipVertical)
        {
            const std::vector<uint8_t> file = ReadFile(filename);
            Decode(bitmap, file.data(), file.size(), flipVertical, filename.string());
            bitmap.SetFilename(filename);
        }

        static void LoadFromMemory(Bitmap &bitmap, const uint8_t *data, size_t size, bool flipVertical = false)
        {
            Decode(bitmap, data, size, flipVertical, "<memory>");
        }

        static void Write(const Bitmap &bitmap, const std::filesystem::path &filename)
        {
            if (!bitmap.GetData())
            {
                throw std::runtime_error("Cannot write empty bitmap");
            }

            const PixelFormat format = bitmap.GetFormat();
            const GLInfo gl          = GetGLInfo(format);

            const uint32_t width  = bitmap.GetSize().x;
            const uint32_t height = bitmap.GetSize().y;
            if (width == 0 || height == 0)
            {
                throw std::runtime_error("Cannot write bitmap with invalid size");
            }

            const size_t bytesPerPixel = PixelFormatBytesPerPixel(format);
            const size_t tightRow      = static_cast<size_t>(width) * bytesPerPixel;
            const size_t paddedRow     = Align4(tightRow);
            const size_t imageSize     = paddedRow * height;

            if (imageSize > 0xFFFFFFFFull)
            {
                throw std::runtime_error("Image too large for KTX1 (imageSize is 32-bit)");
            }

            std::vector<uint8_t> out;
            out.reserve(64 + 4 + imageSize);

            static constexpr uint8_t identifier[12] = {0xAB, 'K',  'T',  'X',  ' ',  '1',
                                                       '1',  0xBB, '\r', '\n', 0x1A, '\n'};
            out.insert(out.end(), identifier, identifier + 12);

            PutU32(out, 0x04030201u); // endianness
            PutU32(out, gl.type);
            PutU32(out, gl.typeSize);
            PutU32(out, gl.format);
            PutU32(out, gl.internalFormat);
            PutU32(out, gl.format); // glBaseInternalFormat
            PutU32(out, width);
            PutU32(out, height);
            PutU32(out, 0); // pixelDepth (2D)
            PutU32(out, 0); // numberOfArrayElements
            PutU32(out, 1); // numberOfFaces
            PutU32(out, 1); // numberOfMipmapLevels
            PutU32(out, 0); // bytesOfKeyValueData

            PutU32(out, static_cast<uint32_t>(imageSize));

            const uint8_t *src = bitmap.GetData().get();
            for (uint32_t y = 0; y < height; ++y)
            {
                out.insert(out.end(), src + y * tightRow, src + (y + 1) * tightRow);
                out.insert(out.end(), paddedRow - tightRow, uint8_t{0}); // rows are padded to 4 bytes
            }

            std::ofstream file(filename, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                throw std::runtime_error("Failed to create KTX file: " + filename.string());
            }
            file.write(reinterpret_cast<const char *>(out.data()), static_cast<std::streamsize>(out.size()));
            if (!file)
            {
                throw std::runtime_error("Failed to write KTX data: " + filename.string());
            }
        }

    private:
        // OpenGL enums used by KTX1 (declared here so no GL headers are needed).
        static constexpr uint32_t GL_UNSIGNED_BYTE  = 0x1401;
        static constexpr uint32_t GL_UNSIGNED_SHORT = 0x1403;
        static constexpr uint32_t GL_FLOAT          = 0x1406;
        static constexpr uint32_t GL_HALF_FLOAT     = 0x140B;

        static constexpr uint32_t GL_RED  = 0x1903;
        static constexpr uint32_t GL_RGB  = 0x1907;
        static constexpr uint32_t GL_RGBA = 0x1908;
        static constexpr uint32_t GL_BGR  = 0x80E0;
        static constexpr uint32_t GL_BGRA = 0x80E1;
        static constexpr uint32_t GL_RG   = 0x8227;

        struct GLInfo
        {
            uint32_t type;
            uint32_t typeSize;
            uint32_t format;
            uint32_t internalFormat;
        };

        static GLInfo GetGLInfo(PixelFormat format)
        {
            static constexpr uint32_t baseFormats[4] = {GL_RED, GL_RG, GL_RGB, GL_RGBA};
            // internal formats per type, indexed by channels - 1
            static constexpr uint32_t internal8[4]   = {0x8229, 0x822B, 0x8051, 0x8058}; // R8 RG8 RGB8 RGBA8
            static constexpr uint32_t internal16[4]  = {0x822A, 0x822C, 0x8054, 0x805B}; // R16 RG16 RGB16 RGBA16
            static constexpr uint32_t internal16F[4] = {0x822D, 0x822F, 0x881B, 0x881A}; // R16F RG16F RGB16F RGBA16F
            static constexpr uint32_t internal32F[4] = {0x822E, 0x8230, 0x8815, 0x8814}; // R32F RG32F RGB32F RGBA32F

            const uint32_t index = PixelFormatChannels(format) - 1;
            switch (PixelFormatType(format))
            {
                case PixelType::UInt8:
                    return {GL_UNSIGNED_BYTE, 1, baseFormats[index], internal8[index]};
                case PixelType::UInt16:
                    return {GL_UNSIGNED_SHORT, 2, baseFormats[index], internal16[index]};
                case PixelType::Float16:
                    return {GL_HALF_FLOAT, 2, baseFormats[index], internal16F[index]};
                case PixelType::Float32:
                    return {GL_FLOAT, 4, baseFormats[index], internal32F[index]};
            }
            throw std::runtime_error("Unsupported pixel format for KTX");
        }

        static void Decode(Bitmap &bitmap, const uint8_t *bytes, size_t size, bool flipVertical,
                           const std::string &what)
        {
            if (!bytes || size < 12)
            {
                throw std::runtime_error("KTX data too small: " + what);
            }

            static constexpr uint8_t ktx2Identifier[12] = {0xAB, 'K',  'T',  'X',  ' ',  '2',
                                                           '0',  0xBB, '\r', '\n', 0x1A, '\n'};
            if (std::memcmp(bytes, ktx2Identifier, 12) == 0)
            {
                throw std::runtime_error("KTX2 files are not supported (ktxparse reads KTX1 only): " + what);
            }

            ktxparse::Texture ktx{bytes, size};
            if (!ktx)
            {
                throw std::runtime_error("Not a valid KTX1 file: " + what);
            }
            if (ktx.isCompressed)
            {
                throw std::runtime_error("Compressed KTX (BCn/ETC/ASTC) cannot be stored in a Bitmap: " + what);
            }
            if (ktx.dimensions > 2)
            {
                throw std::runtime_error("3D KTX textures are not supported: " + what);
            }

            // Take mip 0 of the first array layer / face.
            bool found = false;
            ktxparse::Image base{};
            for (auto img: ktx)
            {
                if (img.level == 0)
                {
                    base  = img;
                    found = true;
                    break;
                }
            }
            if (!found || !base.data)
            {
                throw std::runtime_error("KTX contains no base image: " + what);
            }

            uint32_t channels = 0;
            bool bgr          = false;
            switch (base.format)
            {
                case GL_RED:
                    channels = 1;
                    break;
                case GL_RG:
                    channels = 2;
                    break;
                case GL_RGB:
                    channels = 3;
                    break;
                case GL_RGBA:
                    channels = 4;
                    break;
                case GL_BGR:
                    channels = 3;
                    bgr      = true;
                    break;
                case GL_BGRA:
                    channels = 4;
                    bgr      = true;
                    break;
                default:
                    throw std::runtime_error("Unsupported KTX pixel format 0x" + ToHex(base.format) + ": " + what);
            }

            PixelType type;
            switch (base.type)
            {
                case GL_UNSIGNED_BYTE:
                    type = PixelType::UInt8;
                    break;
                case GL_UNSIGNED_SHORT:
                    type = PixelType::UInt16;
                    break;
                case GL_HALF_FLOAT:
                    type = PixelType::Float16;
                    break;
                case GL_FLOAT:
                    type = PixelType::Float32;
                    break;
                default:
                    throw std::runtime_error("Unsupported KTX component type 0x" + ToHex(base.type) + ": " + what);
            }

            const size_t width         = base.width;
            const size_t height        = base.height > 0 ? base.height : 1; // 1D textures have height 0
            const size_t sampleBytes   = PixelTypeBytes(type);
            const size_t bytesPerPixel = channels * sampleBytes;
            const size_t tightRow      = width * bytesPerPixel;
            const size_t paddedRow     = Align4(tightRow);

            if (width == 0)
            {
                throw std::runtime_error("KTX has zero width: " + what);
            }

            // KTX1 pads rows to 4 bytes; tolerate writers that did not.
            size_t stride;
            if (base.size >= paddedRow * height)
                stride = paddedRow;
            else if (base.size >= tightRow * height)
                stride = tightRow;
            else
                throw std::runtime_error("KTX image data is truncated: " + what);

            auto data = std::make_unique<uint8_t[]>(tightRow * height);
            for (size_t y = 0; y < height; ++y)
            {
                const size_t srcY = flipVertical ? (height - 1 - y) : y;
                std::memcpy(data.get() + y * tightRow, base.data + srcY * stride, tightRow);
            }

            const size_t sampleCount = width * height * channels;
            if (ktx.isByteSwapped && sampleBytes > 1)
            {
                uint8_t *p = data.get();
                for (size_t i = 0; i < sampleCount; ++i, p += sampleBytes)
                    std::reverse(p, p + sampleBytes);
            }

            if (bgr)
            {
                uint8_t *p = data.get();
                for (size_t i = 0; i < width * height; ++i, p += bytesPerPixel)
                {
                    for (size_t b = 0; b < sampleBytes; ++b)
                        std::swap(p[b], p[2 * sampleBytes + b]);
                }
            }

            bitmap.SetData(std::move(data));
            bitmap.SetSize(Ui32Vec2(static_cast<uint32_t>(width), static_cast<uint32_t>(height)));
            bitmap.SetFormat(MakePixelFormat(type, channels));
        }

        static size_t Align4(size_t value) { return (value + 3u) & ~size_t{3}; }

        static void PutU32(std::vector<uint8_t> &out, uint32_t value)
        {
            uint8_t bytes[4];
            std::memcpy(bytes, &value, 4);
            out.insert(out.end(), bytes, bytes + 4);
        }

        static std::vector<uint8_t> ReadFile(const std::filesystem::path &filename)
        {
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file)
            {
                throw std::runtime_error("Failed to open KTX file: " + filename.string());
            }
            const std::streamsize size = file.tellg();
            if (size <= 0)
            {
                throw std::runtime_error("KTX file is empty: " + filename.string());
            }
            std::vector<uint8_t> buffer(static_cast<size_t>(size));
            file.seekg(0);
            file.read(reinterpret_cast<char *>(buffer.data()), size);
            if (!file)
            {
                throw std::runtime_error("Failed to read KTX file: " + filename.string());
            }
            return buffer;
        }

        static inline bool registered = Register("ktx");
    };
} // namespace SF::Engine
