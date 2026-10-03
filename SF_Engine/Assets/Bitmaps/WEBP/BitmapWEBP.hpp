#pragma once
#include <Assets/Bitmaps/Bitmap.hpp>

#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux_types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace SF::Engine
{
    // WebP (lossy + lossless, with alpha) via libwebp.
    //
    // Load:  always returns RGBA8 with straight alpha. Animated files return their first frame.
    // Write: lossless by default (RGB under fully transparent pixels is preserved, so packed-channel
    //        textures survive). Use WriteWithOptions for lossy output. WebP is 8-bit only, so non-8-bit
    //        bitmaps are converted (and clamped) to 8-bit first. Max dimension is 16383.
    class BitmapWEBP : public Bitmap::Registrar<BitmapWEBP>
    {
    public:
        struct EncodeOptions
        {
            bool lossless = true;
            float quality = 90.0f; // lossy: visual quality 0-100; lossless: compression effort 0-100
            int method    = 4;     // 0 (fast) .. 6 (slowest, smallest)
        };

        static void Load(Bitmap &bitmap, const std::filesystem::path &filename)
        {
            const std::vector<uint8_t> file = ReadFile(filename);
            LoadFromMemory(bitmap, file.data(), file.size());
            bitmap.SetFilename(filename);
        }

        static void LoadFromMemory(Bitmap &bitmap, const uint8_t *data, size_t size)
        {
            if (!data || size == 0)
            {
                throw std::runtime_error("WebP data is empty");
            }

            WebPBitstreamFeatures features;
            const VP8StatusCode status = WebPGetFeatures(data, size, &features);
            if (status != VP8_STATUS_OK)
            {
                throw std::runtime_error(std::string("Not a valid WebP image: ") + StatusName(status));
            }
            if (features.width <= 0 || features.height <= 0)
            {
                throw std::runtime_error("WebP has invalid dimensions");
            }

            const size_t width  = static_cast<size_t>(features.width);
            const size_t height = static_cast<size_t>(features.height);
            const size_t stride = width * 4;
            auto pixels         = std::make_unique<uint8_t[]>(stride * height);

            if (features.has_animation)
            {
                DecodeFirstAnimatedFrame(data, size, pixels.get(), stride * height);
            } else if (!WebPDecodeRGBAInto(data, size, pixels.get(), stride * height, static_cast<int>(stride)))
            {
                throw std::runtime_error("Failed to decode WebP image");
            }

            bitmap.SetData(std::move(pixels));
            bitmap.SetSize(UVec2(static_cast<uint32_t>(width), static_cast<uint32_t>(height)));
            bitmap.SetFormat(PixelFormat::RGBA8);
        }

        // Lossless.
        static void Write(const Bitmap &bitmap, const std::filesystem::path &filename)
        {
            WriteWithOptions(bitmap, filename, EncodeOptions{});
        }

        static void WriteWithOptions(const Bitmap &bitmap, const std::filesystem::path &filename,
                                     const EncodeOptions &options)
        {
            if (!bitmap.GetData())
            {
                throw std::runtime_error("Cannot write empty bitmap");
            }

            const uint32_t width  = bitmap.GetSize().x;
            const uint32_t height = bitmap.GetSize().y;
            if (width == 0 || height == 0 || width > static_cast<uint32_t>(WEBP_MAX_DIMENSION) ||
                height > static_cast<uint32_t>(WEBP_MAX_DIMENSION))
            {
                throw std::runtime_error("WebP supports dimensions 1.." + std::to_string(WEBP_MAX_DIMENSION));
            }

            // 2- and 4-channel formats carry alpha; everything else is written as opaque RGB.
            const uint32_t channelCount = bitmap.GetChannelCount();
            const bool hasAlpha         = (channelCount == 2 || channelCount == 4);
            const PixelFormat target    = hasAlpha ? PixelFormat::RGBA8 : PixelFormat::RGB8;

            Bitmap converted;
            const Bitmap *source = &bitmap;
            if (bitmap.GetFormat() != target)
            {
                converted = bitmap.Converted(target);
                source    = &converted;
            }
            const uint8_t *pixels = source->GetData().get();
            const int stride      = static_cast<int>(width * (hasAlpha ? 4u : 3u));

            WebPConfig config;
            if (!WebPConfigInit(&config))
            {
                throw std::runtime_error("WebPConfigInit failed (libwebp version mismatch)");
            }
            config.lossless = options.lossless ? 1 : 0;
            config.exact    = options.lossless ? 1 : 0; // keep RGB values under transparent pixels
            config.quality  = std::clamp(options.quality, 0.0f, 100.0f);
            config.method   = std::clamp(options.method, 0, 6);
            if (!WebPValidateConfig(&config))
            {
                throw std::runtime_error("Invalid WebP encoder configuration");
            }

            WebPPicture picture;
            if (!WebPPictureInit(&picture))
            {
                throw std::runtime_error("WebPPictureInit failed (libwebp version mismatch)");
            }
            PictureGuard pictureGuard{&picture};
            picture.use_argb = options.lossless ? 1 : 0;
            picture.width    = static_cast<int>(width);
            picture.height   = static_cast<int>(height);

            WebPMemoryWriter writer;
            WebPMemoryWriterInit(&writer);
            WriterGuard writerGuard{&writer};
            picture.writer     = WebPMemoryWrite;
            picture.custom_ptr = &writer;

            const int imported = hasAlpha ? WebPPictureImportRGBA(&picture, pixels, stride)
                                          : WebPPictureImportRGB(&picture, pixels, stride);
            if (!imported)
            {
                throw std::runtime_error(std::string("Failed to import pixels into WebP encoder: ") +
                                         EncodeErrorName(picture.error_code));
            }
            if (!WebPEncode(&config, &picture))
            {
                throw std::runtime_error(std::string("WebP encoding failed: ") + EncodeErrorName(picture.error_code));
            }

            std::ofstream file(filename, std::ios::binary | std::ios::trunc);
            if (!file)
            {
                throw std::runtime_error("Failed to create WebP file: " + filename.string());
            }
            file.write(reinterpret_cast<const char *>(writer.mem), static_cast<std::streamsize>(writer.size));
            if (!file)
            {
                throw std::runtime_error("Failed to write WebP data: " + filename.string());
            }
        }

    private:
        struct PictureGuard
        {
            WebPPicture *picture;
            ~PictureGuard() { WebPPictureFree(picture); }
        };

        struct WriterGuard
        {
            WebPMemoryWriter *writer;
            ~WriterGuard() { WebPMemoryWriterClear(writer); }
        };

        // Animated WebP: decode the first composited frame (full canvas, non-premultiplied RGBA).
        static void DecodeFirstAnimatedFrame(const uint8_t *data, size_t size, uint8_t *out, size_t outSize)
        {
            WebPData webpData;
            WebPDataInit(&webpData);
            webpData.bytes = data;
            webpData.size  = size;

            WebPAnimDecoderOptions decoderOptions;
            if (!WebPAnimDecoderOptionsInit(&decoderOptions))
            {
                throw std::runtime_error("WebPAnimDecoderOptionsInit failed (libwebp version mismatch)");
            }
            decoderOptions.color_mode  = MODE_RGBA;
            decoderOptions.use_threads = 0;

            std::unique_ptr<WebPAnimDecoder, decltype(&WebPAnimDecoderDelete)> decoder(
                    WebPAnimDecoderNew(&webpData, &decoderOptions), &WebPAnimDecoderDelete);
            if (!decoder)
            {
                throw std::runtime_error("Failed to create WebP animation decoder");
            }

            WebPAnimInfo info;
            if (!WebPAnimDecoderGetInfo(decoder.get(), &info) ||
                static_cast<size_t>(info.canvas_width) * info.canvas_height * 4 != outSize)
            {
                throw std::runtime_error("WebP animation canvas does not match image dimensions");
            }

            uint8_t *frame = nullptr;
            int timestamp  = 0;
            if (!WebPAnimDecoderGetNext(decoder.get(), &frame, &timestamp) || !frame)
            {
                throw std::runtime_error("Failed to decode first WebP animation frame");
            }
            std::memcpy(out, frame, outSize);
        }

        static std::vector<uint8_t> ReadFile(const std::filesystem::path &filename)
        {
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file)
            {
                throw std::runtime_error("Failed to open WebP file: " + filename.string());
            }
            const std::streamsize size = file.tellg();
            if (size <= 0)
            {
                throw std::runtime_error("WebP file is empty: " + filename.string());
            }
            std::vector<uint8_t> buffer(static_cast<size_t>(size));
            file.seekg(0);
            file.read(reinterpret_cast<char *>(buffer.data()), size);
            if (!file)
            {
                throw std::runtime_error("Failed to read WebP file: " + filename.string());
            }
            return buffer;
        }

        static const char *StatusName(VP8StatusCode status)
        {
            switch (status)
            {
                case VP8_STATUS_OK:
                    return "ok";
                case VP8_STATUS_OUT_OF_MEMORY:
                    return "out of memory";
                case VP8_STATUS_INVALID_PARAM:
                    return "invalid parameter";
                case VP8_STATUS_BITSTREAM_ERROR:
                    return "bitstream error";
                case VP8_STATUS_UNSUPPORTED_FEATURE:
                    return "unsupported feature";
                case VP8_STATUS_SUSPENDED:
                    return "suspended";
                case VP8_STATUS_USER_ABORT:
                    return "user abort";
                case VP8_STATUS_NOT_ENOUGH_DATA:
                    return "not enough data (truncated file?)";
            }
            return "unknown error";
        }

        static const char *EncodeErrorName(WebPEncodingError error)
        {
            switch (error)
            {
                case VP8_ENC_OK:
                    return "ok";
                case VP8_ENC_ERROR_OUT_OF_MEMORY:
                    return "out of memory";
                case VP8_ENC_ERROR_BITSTREAM_OUT_OF_MEMORY:
                    return "out of memory while flushing bits";
                case VP8_ENC_ERROR_NULL_PARAMETER:
                    return "null parameter";
                case VP8_ENC_ERROR_INVALID_CONFIGURATION:
                    return "invalid configuration";
                case VP8_ENC_ERROR_BAD_DIMENSION:
                    return "bad dimension";
                case VP8_ENC_ERROR_PARTITION0_OVERFLOW:
                    return "partition 0 overflow (> 512 KiB)";
                case VP8_ENC_ERROR_PARTITION_OVERFLOW:
                    return "partition overflow (> 16 MiB)";
                case VP8_ENC_ERROR_BAD_WRITE:
                    return "bad write";
                case VP8_ENC_ERROR_FILE_TOO_BIG:
                    return "file too big (> 4 GiB)";
                case VP8_ENC_ERROR_USER_ABORT:
                    return "user abort";
                default:
                    break;
            }
            return "unknown error";
        }

        static inline bool registered = Register("webp");
    };
} // namespace SF::Engine
