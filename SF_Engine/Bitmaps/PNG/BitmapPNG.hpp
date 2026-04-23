#pragma once

#include <png.h>
#include <Bitmaps/Bitmap.hpp>
#include <Files/File.hpp>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace SF::Engine
{
    class BitmapPNG : public Bitmap::Registrar<BitmapPNG>
    {
    public:
        static void Load(Bitmap& bitmap, const std::filesystem::path& filename);
        static void Write(const Bitmap& bitmap, const std::filesystem::path& filename);

    private:
        struct PNGReadContext
        {
            png_structp png = nullptr;
            png_infop info = nullptr;
            File file;

            ~PNGReadContext();
            void cleanup();
        };

        struct PNGWriteContext
        {
            png_structp png = nullptr;
            png_infop info = nullptr;
            File file;

            ~PNGWriteContext();
            void cleanup();
        };

        // Custom I/O callbacks for libpng to work with File
        static void PNGReadCallback(png_structp png_ptr, png_bytep data, png_size_t length);
        static void PNGWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length);
        static void PNGFlushCallback(png_structp png_ptr);

        static inline bool registered = Register("png", "PNG");
    };
}