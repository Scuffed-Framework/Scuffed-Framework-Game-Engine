#pragma once
#include <Assets/Bitmaps/Bitmap.hpp>

#include <tinyexr.h>

#include <algorithm>
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
    using namespace std;
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
        static void Load(Bitmap &bitmap, const filesystem::path &filename)
        {
            const vector<unsigned char> fileData = ReadFile(filename);

            float *rgba     = nullptr;
            int width       = 0;
            int height      = 0;
            const char *err = nullptr;

            const int ret = LoadEXRFromMemory(&rgba, &width, &height, fileData.data(), fileData.size(), &err);
            if (ret != TINYEXR_SUCCESS)
            {
                throw runtime_error("Failed to load EXR '" + filename.string() + "': " + TakeError(err));
            }
            unique_ptr<float, decltype(&free)> rgbaGuard(rgba, &free);

            if (width <= 0 || height <= 0)
            {
                throw runtime_error("Invalid EXR dimensions: " + filename.string());
            }

            const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
            auto data               = make_unique<uint8_t[]>(pixelCount * 4);

            for (size_t i = 0; i < pixelCount * 4; ++i)
            {
                data[i] = FloatToByte(rgba[i]);
            }

            bitmap.SetData(std::move(data));
            bitmap.SetSize(UVec2(width, height));
            bitmap.SetBytesPerPixel(4);
            bitmap.SetFilename(filename);
        }

        // Writes 1/2/3/4 byte-per-pixel bitmaps (gray, gray+alpha, RGB, RGBA) as EXR.
        // saveAsHalf stores 16-bit floats on disk (smaller files; required for B44/B44A to be effective).
        static void Write(const Bitmap &bitmap, const filesystem::path &filename,
                          EXRCompression compression = EXRCompression::ZIP, bool saveAsHalf = false)
        {
            const auto &src = bitmap.GetData();
            if (!src)
            {
                throw runtime_error("Cannot write empty bitmap");
            }

            if (compression == EXRCompression::DWAA || compression == EXRCompression::DWAB)
            {
                throw runtime_error("DWAA/DWAB compression is not supported by tinyexr");
            }

            const auto &size             = bitmap.GetSize();
            const int width              = static_cast<int>(size.x);
            const int height             = static_cast<int>(size.y);
            const uint32_t bytesPerPixel = bitmap.GetBytesPerPixel();

            if (width <= 0 || height <= 0 || bytesPerPixel == 0)
            {
                throw runtime_error("Cannot write bitmap with invalid size or format");
            }

            const bool hasAlpha   = (bytesPerPixel == 2 || bytesPerPixel >= 4);
            const int numChannels = hasAlpha ? 4 : 3;

            // EXR requires channels sorted alphabetically: A, B, G, R.
            const char *channelNames = hasAlpha ? "ABGR" : "BGR";
            const int iA             = hasAlpha ? 0 : -1;
            const int iB             = hasAlpha ? 1 : 0;
            const int iG             = iB + 1;
            const int iR             = iB + 2;

            const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
            vector<vector<float>> planes(numChannels, vector<float>(pixelCount));

            for (size_t p = 0; p < pixelCount; ++p)
            {
                const size_t s = p * bytesPerPixel;
                float r, g, b, a = 1.0f;

                if (bytesPerPixel <= 2)
                {
                    r = g = b = src[s] / 255.0f;
                    if (bytesPerPixel == 2)
                        a = src[s + 1] / 255.0f;
                } else
                {
                    r = src[s + 0] / 255.0f;
                    g = src[s + 1] / 255.0f;
                    b = src[s + 2] / 255.0f;
                    if (bytesPerPixel >= 4)
                        a = src[s + 3] / 255.0f;
                }

                planes[iR][p] = r;
                planes[iG][p] = g;
                planes[iB][p] = b;
                if (iA >= 0)
                    planes[iA][p] = a;
            }

            EXRHeader header;
            InitEXRHeader(&header);
            EXRImage image;
            InitEXRImage(&image);

            vector<float *> planePtrs(numChannels);
            for (int i = 0; i < numChannels; ++i)
                planePtrs[i] = planes[i].data();

            image.num_channels = numChannels;
            image.images       = reinterpret_cast<unsigned char **>(planePtrs.data());
            image.width        = width;
            image.height       = height;

            vector<EXRChannelInfo> channelInfos(numChannels);
            vector<int> pixelTypes(numChannels, TINYEXR_PIXELTYPE_FLOAT); // type of the data we pass in
            vector<int> requestedTypes(numChannels,
                                       saveAsHalf ? TINYEXR_PIXELTYPE_HALF : TINYEXR_PIXELTYPE_FLOAT); // type on disk

            for (int i = 0; i < numChannels; ++i)
            {
                memset(&channelInfos[i], 0, sizeof(EXRChannelInfo));
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

            // Encode to memory, then write with ofstream so non-ASCII paths work on Windows.
            unsigned char *encoded   = nullptr;
            const char *err          = nullptr;
            const size_t encodedSize = SaveEXRImageToMemory(&image, &header, &encoded, &err);
            if (encodedSize == 0)
            {
                throw runtime_error("Failed to encode EXR: " + TakeError(err));
            }
            unique_ptr<unsigned char, decltype(&free)> encodedGuard(encoded, &free);

            ofstream file(filename, ios::binary | ios::trunc);
            if (!file)
            {
                throw runtime_error("Failed to create EXR file: " + filename.string());
            }
            file.write(reinterpret_cast<const char *>(encoded), static_cast<streamsize>(encodedSize));
            if (!file)
            {
                throw runtime_error("Failed to write EXR data: " + filename.string());
            }
        }

    private:
        static uint8_t FloatToByte(float v)
        {
            if (!(v > 0.0f)) // also catches NaN
                return 0;
            return static_cast<uint8_t>(min(v, 1.0f) * 255.0f + 0.5f);
        }

        static vector<unsigned char> ReadFile(const filesystem::path &filename)
        {
            ifstream file(filename, ios::binary | ios::ate);
            if (!file)
            {
                throw runtime_error("Failed to open EXR file: " + filename.string());
            }
            const streamsize size = file.tellg();
            if (size <= 0)
            {
                throw runtime_error("EXR file is empty: " + filename.string());
            }
            vector<unsigned char> buffer(static_cast<size_t>(size));
            file.seekg(0);
            file.read(reinterpret_cast<char *>(buffer.data()), size);
            if (!file)
            {
                throw runtime_error("Failed to read EXR file: " + filename.string());
            }
            return buffer;
        }

        static string TakeError(const char *err)
        {
            string message = err ? err : "unknown error";
            if (err)
                FreeEXRErrorMessage(err);
            return message;
        }

        static inline bool registered = Register("exr", "EXR");
    };
} // namespace SF::Engine
