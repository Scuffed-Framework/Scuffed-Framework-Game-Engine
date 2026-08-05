#pragma once
#include <Bitmaps/Bitmap.hpp>
#include <libraw/libraw.h>
#include <filesystem>
#include <stdexcept>
#include <memory>

namespace SF::Engine
{
    class BitmapDNG : public Bitmap::Registrar<BitmapDNG>
    {
    public:
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename)
        {
            LibRaw raw;

            //  Open
            int err = raw.open_file(filename.string().c_str());
            if (err != LIBRAW_SUCCESS)
                throw std::runtime_error("LibRaw failed to open: " + filename.string() + " - " + libraw_strerror(err));

            //  Processing params
            // Output as 8-bit sRGB RGBA, auto white-balance from the camera metadata.
            raw.imgdata.params.output_bps = 8;     // 8 bits per channel
            raw.imgdata.params.output_color = 1;   // sRGB
            raw.imgdata.params.use_auto_wb = 1;    // camera auto white-balance
            raw.imgdata.params.use_camera_wb = 1;  // prefer camera WB if available
            raw.imgdata.params.no_auto_bright = 1; // don't clip highlights
            raw.imgdata.params.highlight = 0;      // clip highlights
            raw.imgdata.params.half_size = 0;      // full resolution
            raw.imgdata.params.four_color_rgb = 0; // standard 3-channel demosaic

            //  Unpack raw data
            err = raw.unpack();
            if (err != LIBRAW_SUCCESS)
                throw std::runtime_error("LibRaw unpack failed: " + std::string(libraw_strerror(err)));

            //  Demosaic + colour pipeline
            err = raw.dcraw_process();
            if (err != LIBRAW_SUCCESS)
                throw std::runtime_error("LibRaw process failed: " + std::string(libraw_strerror(err)));

            //  Pull out the processed image
            int errCode = 0;
            libraw_processed_image_t *img = raw.dcraw_make_mem_image(&errCode);
            if (!img || errCode != LIBRAW_SUCCESS)
                throw std::runtime_error("LibRaw make_mem_image failed: " + std::string(libraw_strerror(errCode)));

            // RAII guard so we always call LibRaw::dcraw_clear_mem
            struct ImgGuard
            {
                libraw_processed_image_t *p;
                ~ImgGuard() { LibRaw::dcraw_clear_mem(p); }
            } guard{img};

            if (img->type != LIBRAW_IMAGE_BITMAP)
                throw std::runtime_error("LibRaw returned non-bitmap image type");

            const uint32_t width = img->width;
            const uint32_t height = img->height;
            const uint32_t channels = img->colors; // 3 (RGB)
            const size_t pixels = width * height;

            //  Convert RGB → RGBA
            auto rgba = std::make_unique<uint8_t[]>(pixels * 4);
            const uint8_t *src = img->data;

            if (channels == 3)
            {
                for (size_t i = 0; i < pixels; ++i)
                {
                    rgba[i * 4 + 0] = src[i * 3 + 0]; // R
                    rgba[i * 4 + 1] = src[i * 3 + 1]; // G
                    rgba[i * 4 + 2] = src[i * 3 + 2]; // B
                    rgba[i * 4 + 3] = 0xFF;           // A = opaque
                }
            }
            else if (channels == 4)
            {
                std::memcpy(rgba.get(), src, pixels * 4);
            }
            else
            {
                throw std::runtime_error("Unexpected channel count from LibRaw: " + std::to_string(channels));
            }

            bitmap.SetData(std::move(rgba));
            bitmap.SetSize(UVec2(width, height));
            bitmap.SetBytesPerPixel(4);
            bitmap.SetFilename(filename);
        }

        // DNG write is intentionally not implemented.
        // DNG is a camera RAW archival format
        // writing processed RGBA back
        // into a valid DNG would require embedding a full TIFF structure,
        // Bayer pattern metadata, colour matrices, and EXIF. Use TIFF or PNG
        // for round-trip storage of processed bitmaps.
        static void Write(const Bitmap &, const std::filesystem::path &filename,
                          int = 0)
        {
            throw std::runtime_error(
                "DNG write is not supported. "
                "Save processed images as PNG or TIFF instead.");
        }

    private:
        static inline bool registered = Register("dng", "DNG");
    };
}