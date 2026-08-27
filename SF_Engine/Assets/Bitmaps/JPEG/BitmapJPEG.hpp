#pragma once
#include <LowLevel/stb_image.h>
#include <LowLevel/stb_image_write.h>
#include <Assets/Bitmaps/Bitmap.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace SF::Engine
{
    class BitmapJPEG : public Bitmap::Registrar<BitmapJPEG>
    {
    public:
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename)
        {
            int width, height, channels;

            // Load JPEG using stb_image
            unsigned char *imageData =
                stbi_load(filename.string().c_str(), &width, &height, &channels,
                          STBI_rgb_alpha // Force 4 channels (RGBA)
                );

            if (!imageData)
            {
                throw std::runtime_error("Failed to load JPEG: " + filename.string());
            }

            // Calculate total size
            size_t dataSize = width * height * 4; // 4 bytes per pixel (RGBA)

            // Transfer ownership to unique_ptr
            auto data = std::make_unique<uint8_t[]>(dataSize);
            std::memcpy(data.get(), imageData, dataSize);

            // Free stb_image data
            stbi_image_free(imageData);

            // Set bitmap properties
            bitmap.SetData(std::move(data));
            bitmap.SetSize(UVec2(width, height));
            bitmap.SetBytesPerPixel(4);
            bitmap.SetFilename(filename);
        }

        static void Write(const Bitmap &bitmap, const std::filesystem::path &filename,
                          int quality = 90)
        {
            if (!bitmap.GetData())
            {
                throw std::runtime_error("Cannot write empty bitmap");
            }

            // Clamp quality to valid range
            quality = std::clamp(quality, 1, 100);

            const auto &size = bitmap.GetSize();
            int width = static_cast<int>(size.x);
            int height = static_cast<int>(size.y);
            int channels = static_cast<int>(bitmap.GetBytesPerPixel());

            // Write JPEG with specified quality
            int result = stbi_write_jpg(filename.string().c_str(), width, height, channels,
                                        bitmap.GetData().get(), quality);

            if (!result)
            {
                throw std::runtime_error("Failed to write JPEG: " + filename.string());
            }
        }

    private:
        // Register JPEG extensions
        static inline bool registered = Register("jpg", "jpeg", "JPG", "JPEG");
    };
}