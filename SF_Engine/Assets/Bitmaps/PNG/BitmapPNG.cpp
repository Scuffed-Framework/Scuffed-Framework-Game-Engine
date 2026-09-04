#include "BitmapPNG.hpp"
#include <cstring>

namespace SF::Engine
{
    // RAII context cleanup
    BitmapPNG::PNGReadContext::~PNGReadContext()
    {
        cleanup();
    }

    void BitmapPNG::PNGReadContext::cleanup()
    {
        if (png || info)
        {
            png_destroy_read_struct(&png, &info, nullptr);
            png = nullptr;
            info = nullptr;
        }
        if (file.IsOpen())
        {
            file.Close();
        }
    }

    BitmapPNG::PNGWriteContext::~PNGWriteContext()
    {
        cleanup();
    }

    void BitmapPNG::PNGWriteContext::cleanup()
    {
        if (png || info)
        {
            png_destroy_write_struct(&png, &info);
            png = nullptr;
            info = nullptr;
        }
        if (file.IsOpen())
        {
            file.Close();
        }
    }

    // Custom I/O callbacks
    void BitmapPNG::PNGReadCallback(png_structp png_ptr, png_bytep data, png_size_t length)
    {
        File* file = static_cast<File*>(png_get_io_ptr(png_ptr));
        if (file->Read(data, length) != length)
        {
            png_error(png_ptr, "Read error");
        }
    }

    void BitmapPNG::PNGWriteCallback(png_structp png_ptr, png_bytep data, png_size_t length)
    {
        File* file = static_cast<File*>(png_get_io_ptr(png_ptr));
        if (file->Write(data, length) != length)
        {
            png_error(png_ptr, "Write error");
        }
    }

    void BitmapPNG::PNGFlushCallback(png_structp png_ptr)
    {
        // File class handles flushing internally, nothing to do here
    }

    void BitmapPNG::PNGMemoryReadCallback(png_structp png_ptr, png_bytep data, png_size_t length)
    {
        MemoryReadState* state = static_cast<MemoryReadState*>(png_get_io_ptr(png_ptr));

        if (state->offset + length > state->size)
        {
            png_error(png_ptr, "Read error: attempted to read past end of memory buffer");
        }

        std::memcpy(data, state->data + state->offset, length);
        state->offset += length;
    }

    void BitmapPNG::DecodeFromLibpng(Bitmap& bitmap, png_structp png, png_infop info,
                                     const std::filesystem::path& filenameForErrors)
    {
        int width = png_get_image_width(png, info);
        int height = png_get_image_height(png, info);
        png_byte color_type = png_get_color_type(png, info);
        png_byte bit_depth = png_get_bit_depth(png, info);

        // Transform to RGBA8
        if (bit_depth == 16) png_set_strip_16(png);

        if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);

        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
            png_set_expand_gray_1_2_4_to_8(png);

        if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);

        if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
            color_type == PNG_COLOR_TYPE_PALETTE)
            png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

        if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);

        png_read_update_info(png, info);

        size_t rowbytes = png_get_rowbytes(png, info);
        std::unique_ptr<png_bytep[]> row_pointers(new png_bytep[height]);

        for (int y = 0; y < height; y++) row_pointers[y] = new png_byte[rowbytes];

        png_read_image(png, row_pointers.get());
        constexpr uint32_t bytesPerPixel = 4;
        auto data = std::make_unique<uint8_t[]>(width * height * bytesPerPixel);

        for (int y = 0; y < height; y++)
        {
            memcpy(data.get() + y * width * bytesPerPixel, row_pointers[y], width * bytesPerPixel);
            delete[] row_pointers[y];
        }
        bitmap.SetData(std::move(data));
        bitmap.SetSize(UVec2(width, height));
        bitmap.SetBytesPerPixel(bytesPerPixel);
        bitmap.SetFilename(filenameForErrors);
    }

    void BitmapPNG::Load(Bitmap& bitmap, const std::filesystem::path& filename)
    {
        PNGReadContext ctx;

        // Open file
        ctx.file = File(filename);
        if (!ctx.file.Open(FileMode::Read))
        {
            throw std::runtime_error("Failed to open PNG file: " + filename.string());
        }

        // Verify PNG signature
        png_byte header[8];
        if (ctx.file.Read(header, 8) != 8 || png_sig_cmp(header, 0, 8))
        {
            throw std::runtime_error("File is not a valid PNG: " + filename.string());
        }

        // Create read struct
        ctx.png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!ctx.png) throw std::runtime_error("Failed to create PNG read struct");

        ctx.info = png_create_info_struct(ctx.png);
        if (!ctx.info) throw std::runtime_error("Failed to create PNG info struct");

        // Set up error handling
        if (setjmp(png_jmpbuf(ctx.png)))
        {
            throw std::runtime_error("Error reading PNG file: " + filename.string());
        }

        // Set custom I/O
        png_set_read_fn(ctx.png, &ctx.file, PNGReadCallback);
        png_set_sig_bytes(ctx.png, 8);
        png_read_info(ctx.png, ctx.info);

        DecodeFromLibpng(bitmap, ctx.png, ctx.info, filename);
    }

    void BitmapPNG::LoadFromMemory(Bitmap& bitmap, const uint8_t* pngData, size_t size)
    {
        if (!pngData || size < 8)
        {
            throw std::runtime_error("LoadFromMemory: buffer too small or null");
        }

        if (png_sig_cmp(const_cast<png_bytep>(pngData), 0, 8))
        {
            throw std::runtime_error("LoadFromMemory: buffer is not a valid PNG");
        }

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png) throw std::runtime_error("Failed to create PNG read struct");

        png_infop info = png_create_info_struct(png);
        if (!info)
        {
            png_destroy_read_struct(&png, nullptr, nullptr);
            throw std::runtime_error("Failed to create PNG info struct");
        }

        if (setjmp(png_jmpbuf(png)))
        {
            png_destroy_read_struct(&png, &info, nullptr);
            throw std::runtime_error("Error decoding PNG from memory");
        }

        MemoryReadState state{pngData, size, 8}; // we already consumed the 8-byte signature
        png_set_read_fn(png, &state, PNGMemoryReadCallback);
        png_set_sig_bytes(png, 8);
        png_read_info(png, info);

        DecodeFromLibpng(bitmap, png, info, std::filesystem::path{}); // no filename for embedded data

        png_destroy_read_struct(&png, &info, nullptr);
    }

    void BitmapPNG::Write(const Bitmap& bitmap, const std::filesystem::path& filename)
    {
        if (!bitmap.GetData()) throw std::runtime_error("Cannot write empty bitmap");

        if (bitmap.GetBytesPerPixel() != 4)
            throw std::runtime_error("PNG writer only supports RGBA format (4 bytes per pixel)");

        PNGWriteContext ctx;

        // Open file
        ctx.file = File(filename);
        if (!ctx.file.Open(FileMode::Write))
        {
            throw std::runtime_error("Failed to create PNG file: " + filename.string());
        }

        // Create write struct
        ctx.png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!ctx.png) throw std::runtime_error("Failed to create PNG write struct");

        ctx.info = png_create_info_struct(ctx.png);
        if (!ctx.info) throw std::runtime_error("Failed to create PNG info struct");

        // Set up error handling
        if (setjmp(png_jmpbuf(ctx.png)))
        {
            throw std::runtime_error("Error writing PNG file: " + filename.string());
        }

        // Set custom I/O
        png_set_write_fn(ctx.png, &ctx.file, PNGWriteCallback, PNGFlushCallback);

        // Set image parameters
        const auto& size = bitmap.GetSize();
        png_set_IHDR(ctx.png, ctx.info, size.x, size.y, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        png_write_info(ctx.png, ctx.info);

        // Create row pointers from contiguous data
        const uint8_t* data = bitmap.GetData().get();
        std::unique_ptr<png_bytep[]> row_pointers(new png_bytep[size.y]);

        for (uint32_t y = 0; y < size.y; y++)
            row_pointers[y] = const_cast<png_bytep>(data + y * size.x * bitmap.GetBytesPerPixel());

        // Write image data
        png_write_image(ctx.png, row_pointers.get());
        png_write_end(ctx.png, nullptr);
    }
}