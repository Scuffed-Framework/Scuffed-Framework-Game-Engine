#pragma once
#include <Assets/Bitmaps/Bitmap.hpp>

#include <tinyexr.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace SF::Engine
{
    // Values match the EXR spec and tinyexr's TINYEXR_COMPRESSIONTYPE_* constants.
    enum class EXRCompression
    {
        NONE  = 0,
        RLE   = 1,
        ZIPS  = 2,
        ZIP   = 3,
        PIZ   = 4,
        PXR24 = 5,
        B44   = 6,
        B44A  = 7,
        DWAA  = 8, // not supported by tinyexr
        DWAB  = 9  // not supported by tinyexr
    };

    class BitmapEXR : public Bitmap::Registrar<BitmapEXR>
    {
    public:
        // Loads any single-part scanline/tiled EXR (all tinyexr-supported compressions) as RGBA32F.
        // Full HDR range is preserved: nothing is clamped or tonemapped.
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename)
        {
            const std::vector<unsigned char> fileData = ReadFile(filename);

            float *rgba     = nullptr;
            int width       = 0;
            int height      = 0;
            const char *err = nullptr;

            const int ret = LoadEXRFromMemory(&rgba, &width, &height, fileData.data(), fileData.size(), &err);
            if (ret != TINYEXR_SUCCESS)
            {
                throw std::runtime_error("Failed to load EXR '" + filename.string() + "': " + TakeError(err));
            }
            std::unique_ptr<float, decltype(&std::free)> rgbaGuard(rgba, &std::free);

            if (width <= 0 || height <= 0)
            {
                throw std::runtime_error("Invalid EXR dimensions: " + filename.string());
            }

            const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4 * sizeof(float);
            auto data              = std::make_unique<uint8_t[]>(byteCount);
            std::memcpy(data.get(), rgba, byteCount);

            bitmap.SetData(std::move(data));
            bitmap.SetSize(UVec2(static_cast<uint32_t>(width), static_cast<uint32_t>(height)));
            bitmap.SetFormat(PixelFormat::RGBA32F);
            bitmap.SetFilename(filename);
        }

        // Writes a bitmap of any pixel format. Data is converted to float; alpha is written only for
        // 4-channel formats, and 1/2-channel bitmaps are expanded to RGB (see Bitmap::Converted).
        // saveAsHalf stores 16-bit floats on disk (smaller files; use it with B44/B44A).
        static void Write(const Bitmap &bitmap, const std::filesystem::path &filename,
                          EXRCompression compression = EXRCompression::ZIP, bool saveAsHalf = false)
        {
            if (!bitmap.GetData())
            {
                throw std::runtime_error("Cannot write empty bitmap");
            }

            if (compression == EXRCompression::DWAA || compression == EXRCompression::DWAB)
            {
                throw std::runtime_error("DWAA/DWAB compression is not supported by tinyexr");
            }

            const auto &size = bitmap.GetSize();
            const int width  = static_cast<int>(size.x);
            const int height = static_cast<int>(size.y);
            if (width <= 0 || height <= 0)
            {
                throw std::runtime_error("Cannot write bitmap with invalid size");
            }

            const bool hasAlpha           = bitmap.GetChannelCount() == 4;
            const PixelFormat floatFormat = hasAlpha ? PixelFormat::RGBA32F : PixelFormat::RGB32F;
            const uint32_t srcChannels    = hasAlpha ? 4u : 3u;

            Bitmap converted;
            const Bitmap *source = &bitmap;
            if (bitmap.GetFormat() != floatFormat)
            {
                converted = bitmap.Converted(floatFormat);
                source    = &converted;
            }
            const uint8_t *srcBytes = source->GetData().get();

            // EXR requires channels sorted alphabetically: A, B, G, R.
            const int numChannels    = static_cast<int>(srcChannels);
            const char *channelNames = hasAlpha ? "ABGR" : "BGR";
            const int iA             = hasAlpha ? 0 : -1;
            const int iB             = hasAlpha ? 1 : 0;
            const int iG             = iB + 1;
            const int iR             = iB + 2;

            const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
            std::vector<std::vector<float>> planes(numChannels, std::vector<float>(pixelCount));

            for (size_t p = 0; p < pixelCount; ++p)
            {
                float px[4];
                std::memcpy(px, srcBytes + p * srcChannels * sizeof(float), srcChannels * sizeof(float));
                planes[iR][p] = px[0];
                planes[iG][p] = px[1];
                planes[iB][p] = px[2];
                if (iA >= 0)
                    planes[iA][p] = px[3];
            }

            // --- tinyexr structures ---
            EXRHeader header;
            InitEXRHeader(&header);
            EXRImage image;
            InitEXRImage(&image);

            std::vector<float *> planePtrs(numChannels);
            for (int i = 0; i < numChannels; ++i)
                planePtrs[i] = planes[i].data();

            image.num_channels = numChannels;
            image.images       = reinterpret_cast<unsigned char **>(planePtrs.data());
            image.width        = width;
            image.height       = height;

            std::vector<EXRChannelInfo> channelInfos(numChannels);
            std::vector<int> pixelTypes(numChannels, TINYEXR_PIXELTYPE_FLOAT); // type of the data we pass in
            std::vector<int> requestedTypes(numChannels, saveAsHalf ? TINYEXR_PIXELTYPE_HALF
                                                                    : TINYEXR_PIXELTYPE_FLOAT); // type on disk

            for (int i = 0; i < numChannels; ++i)
            {
                std::memset(&channelInfos[i], 0, sizeof(EXRChannelInfo));
                channelInfos[i].name[0]    = channelNames[i];
                channelInfos[i].name[1]    = '\0';
                channelInfos[i].x_sampling = 1;
                channelInfos[i].y_sampling = 1;
            }

            header.num_channels          = numChannels;
            header.channels              = channelInfos.data();
            header.pixel_types           = pixelTypes.data();
            header.requested_pixel_types = requestedTypes.data();
            header.compression_type      = static_cast<int>(compression);

            // Encode to memory, then write with std::ofstream so non-ASCII paths work on Windows.
            unsigned char *encoded   = nullptr;
            const char *err          = nullptr;
            const size_t encodedSize = SaveEXRImageToMemory(&image, &header, &encoded, &err);
            if (encodedSize == 0)
            {
                throw std::runtime_error("Failed to encode EXR: " + TakeError(err));
            }
            std::unique_ptr<unsigned char, decltype(&std::free)> encodedGuard(encoded, &std::free);

            std::ofstream file(filename, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                throw std::runtime_error("Failed to create EXR file: " + filename.string());
            }
            file.write(reinterpret_cast<const char *>(encoded), static_cast<std::streamsize>(encodedSize));
            if (!file)
            {
                throw std::runtime_error("Failed to write EXR data: " + filename.string());
            }
        }

    private:
        static std::vector<unsigned char> ReadFile(const std::filesystem::path &filename)
        {
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file)
            {
                throw std::runtime_error("Failed to open EXR file: " + filename.string());
            }
            const std::streamsize size = file.tellg();
            if (size <= 0)
            {
                throw std::runtime_error("EXR file is empty: " + filename.string());
            }
            std::vector<unsigned char> buffer(static_cast<size_t>(size));
            file.seekg(0);
            file.read(reinterpret_cast<char *>(buffer.data()), size);
            if (!file)
            {
                throw std::runtime_error("Failed to read EXR file: " + filename.string());
            }
            return buffer;
        }

        static std::string TakeError(const char *err)
        {
            std::string message = err ? err : "unknown error";
            if (err)
                FreeEXRErrorMessage(err);
            return message;
        }

        static inline bool registered = Register("exr");
    };
} // namespace SF::Engine
