#include "Bitmap.hpp"

namespace SF::Engine
{
    Bitmap::Bitmap(std::filesystem::path filename)
    {
        Load(std::move(filename));
    }

    Bitmap::Bitmap(const UVec2& size, uint32_t bytesPerPixel)
        : size(size), bytesPerPixel(bytesPerPixel)
    {
        data = std::make_unique<uint8_t[]>(CalculateLength(size, bytesPerPixel));
    }

    Bitmap::Bitmap(std::unique_ptr<uint8_t[]>&& data, const UVec2& size,
                   uint32_t bytesPerPixel)
        : data(std::move(data)), size(size), bytesPerPixel(bytesPerPixel)
    {
    }

    void Bitmap::Load(const std::filesystem::path& filename)
    {
        // Implementation using Registry()
    }

    void Bitmap::Write(const std::filesystem::path& filename) const
    {
        // Implementation using Registry()
    }

    uint32_t Bitmap::GetLength() const
    {
        return CalculateLength(size, bytesPerPixel);
    }

    uint32_t Bitmap::CalculateLength(const UVec2& size, uint32_t bytesPerPixel)
    {
        return size.x * size.y * bytesPerPixel;
    }
}