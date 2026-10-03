#pragma once
#include <Assets/Bitmaps/Bitmap.hpp>

#include <libraw/libraw.h>

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
    // Loads camera RAW files (CR2, CR3, NEF, ARW, DNG, ORF, RAF, RW2, PEF, ...) through LibRaw.
    // Camera RAW is a read-only format: Write() always throws.
    class BitmapRAW : public Bitmap::Registrar<BitmapRAW>
    {
    public:
        struct DevelopOptions
        {
            bool output16Bit           = true;  // RGBA16 instead of RGBA8
            bool linearOutput          = false; // no gamma curve (scene-linear) instead of sRGB-style output
            bool halfSize              = false; // 2x downscale during demosaic (much faster, good for previews)
            bool useCameraWhiteBalance = true;  // use the white balance recorded by the camera
            bool autoBrightness        = false; // dcraw-style auto brightness (clips the top 1%)
        };

        // Default: 16-bit sRGB RGBA16.
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename)
        {
            LoadWithOptions(bitmap, filename, DevelopOptions{});
        }

        // Demosaics to RGBA16 (or RGBA8), honoring the camera's orientation flag.
        static void LoadWithOptions(Bitmap &bitmap, const std::filesystem::path &filename,
                                    const DevelopOptions &options)
        {
            const std::vector<char> fileData = ReadFile(filename);

            LibRaw processor;
            auto &params = processor.imgdata.params;

            params.output_bps     = options.output16Bit ? 16 : 8;
            params.output_color   = 1; // sRGB primaries
            params.use_camera_wb  = options.useCameraWhiteBalance ? 1 : 0;
            params.no_auto_bright = options.autoBrightness ? 0 : 1;
            params.half_size      = options.halfSize ? 1 : 0;
            if (options.linearOutput)
            {
                params.gamm[0] = 1.0;
                params.gamm[1] = 1.0;
            }

            // open_buffer instead of open_file so non-ASCII paths work on Windows.
            Check(processor.open_buffer(fileData.data(), fileData.size()), "open", filename);
            Check(processor.unpack(), "unpack", filename);
            Check(processor.dcraw_process(), "process", filename);

            int err = 0;
            std::unique_ptr<libraw_processed_image_t, void (*)(libraw_processed_image_t *)> image(
                    processor.dcraw_make_mem_image(&err),
                    [](libraw_processed_image_t *p) { LibRaw::dcraw_clear_mem(p); });

            if (!image)
            {
                Check(err != LIBRAW_SUCCESS ? err : LIBRAW_UNSPECIFIED_ERROR, "make image", filename);
            }

            const int expectedBits = options.output16Bit ? 16 : 8;
            if (image->type != LIBRAW_IMAGE_BITMAP || image->bits != expectedBits || image->width == 0 ||
                image->height == 0)
            {
                throw std::runtime_error("Unexpected RAW output format: " + filename.string());
            }

            const size_t width      = image->width;
            const size_t height     = image->height;
            const size_t colors     = image->colors;
            const size_t pixelCount = width * height;
            const size_t sampleSize = options.output16Bit ? sizeof(uint16_t) : sizeof(uint8_t);

            if (colors != 1 && colors != 3 && colors != 4)
            {
                throw std::runtime_error("Unsupported RAW channel count: " + filename.string());
            }
            if (image->data_size < pixelCount * colors * sampleSize)
            {
                throw std::runtime_error("RAW output buffer is truncated: " + filename.string());
            }

            auto data = std::make_unique<uint8_t[]>(pixelCount * 4 * sampleSize);
            if (options.output16Bit)
                ExpandToRGBA<uint16_t>(image->data, data.get(), pixelCount, colors, 65535);
            else
                ExpandToRGBA<uint8_t>(image->data, data.get(), pixelCount, colors, 255);

            bitmap.SetData(std::move(data));
            bitmap.SetSize(UVec2(static_cast<uint32_t>(width), static_cast<uint32_t>(height)));
            bitmap.SetFormat(options.output16Bit ? PixelFormat::RGBA16 : PixelFormat::RGBA8);
            bitmap.SetFilename(filename);
        }

        static void Write(const Bitmap &, const std::filesystem::path &filename)
        {
            throw std::runtime_error("Writing camera RAW files is not supported: " + filename.string());
        }

    private:
        // Samples from LibRaw are host-endian; expands 1/3/4 channels to RGBA.
        template<typename T>
        static void ExpandToRGBA(const unsigned char *src, uint8_t *dst, size_t pixelCount, size_t colors, T opaque)
        {
            for (size_t i = 0; i < pixelCount; ++i)
            {
                T s[4] = {};
                std::memcpy(s, src + i * colors * sizeof(T), colors * sizeof(T));

                const T px[4] = {s[0], (colors >= 3) ? s[1] : s[0], (colors >= 3) ? s[2] : s[0],
                                 (colors == 4) ? s[3] : opaque};
                std::memcpy(dst + i * 4 * sizeof(T), px, sizeof(px));
            }
        }

        static std::vector<char> ReadFile(const std::filesystem::path &filename)
        {
            std::ifstream file(filename, std::ios::binary | std::ios::ate);
            if (!file)
            {
                throw std::runtime_error("Failed to open RAW file: " + filename.string());
            }
            const std::streamsize size = file.tellg();
            if (size <= 0)
            {
                throw std::runtime_error("RAW file is empty: " + filename.string());
            }
            std::vector<char> buffer(static_cast<size_t>(size));
            file.seekg(0);
            file.read(buffer.data(), size);
            if (!file)
            {
                throw std::runtime_error("Failed to read RAW file: " + filename.string());
            }
            return buffer;
        }

        static void Check(int code, const char *stage, const std::filesystem::path &filename)
        {
            if (code != LIBRAW_SUCCESS)
            {
                throw std::runtime_error(std::string("LibRaw ") + stage + " failed for '" + filename.string() +
                                         "': " + libraw_strerror(code));
            }
        }

        // The registry normalizes extensions to lowercase, so only lowercase names are needed.
        static inline bool registered = Register("cr2", "cr3", "crw", "nef", "nrw", "arw", "srf", "sr2", "dng", "orf",
                                                 "raf", "rw2", "pef", "srw", "3fr", "x3f", "iiq", "raw");
    };
} // namespace SF::Engine
