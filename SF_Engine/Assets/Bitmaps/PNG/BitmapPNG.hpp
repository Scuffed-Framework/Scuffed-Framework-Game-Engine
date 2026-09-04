#pragma once

#include <png.h>
#include <Assets/Bitmaps/Bitmap.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace SF::Engine
{
    class BitmapPNG : public Bitmap::Registrar<BitmapPNG>
    {
    public:
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename);
        static void Write(const Bitmap &bitmap, const std::filesystem::path &filename);
        static void LoadFromMemory(Bitmap &bitmap, const uint8_t *pngData, size_t size);

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

        struct MemoryReadState
        {
            const uint8_t *data = nullptr;
            size_t size = 0;
            size_t offset = 0;
        };

        static void PNGReadCallback(png_structp png_ptr, png_bytep data, png_size_t length);
        static void PNGWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length);
        static void PNGFlushCallback(png_structp png_ptr);

        static void PNGMemoryReadCallback(png_structp png_ptr, png_bytep data, png_size_t length);

        static void DecodeFromLibpng(Bitmap &bitmap, png_structp png, png_infop info,
                                     const std::filesystem::path &filenameForErrors);

        static inline bool registered = Register("png", "PNG");
    };
}