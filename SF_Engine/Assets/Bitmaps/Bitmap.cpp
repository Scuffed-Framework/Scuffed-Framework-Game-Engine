#include "Bitmap.hpp"

#include <bit>
#include <cstring>
#include <stdexcept>

namespace SF::Engine
{
    namespace
    {
        float HalfToFloat(uint16_t h)
        {
            const uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
            uint32_t exponent   = (h >> 10) & 0x1Fu;
            uint32_t mantissa   = h & 0x3FFu;
            uint32_t bits;

            if (exponent == 0)
            {
                if (mantissa == 0)
                {
                    bits = sign; // +-0
                } else
                {
                    // Subnormal half -> normal float.
                    exponent = 113;
                    while (!(mantissa & 0x400u))
                    {
                        mantissa <<= 1;
                        --exponent;
                    }
                    mantissa &= 0x3FFu;
                    bits = sign | (exponent << 23) | (mantissa << 13);
                }
            } else if (exponent == 31)
            {
                bits = sign | 0x7F800000u | (mantissa << 13); // inf / NaN
            } else
            {
                bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
            }
            return std::bit_cast<float>(bits);
        }

        uint16_t FloatToHalf(float value)
        {
            const auto x        = std::bit_cast<uint32_t>(value);
            const uint32_t sign = (x >> 16) & 0x8000u;
            const uint32_t absx = x & 0x7FFFFFFFu;

            if (absx >= 0x7F800000u) // inf / NaN
                return static_cast<uint16_t>(sign | 0x7C00u | (absx > 0x7F800000u ? 0x200u : 0u));

            if (absx >= 0x477FF000u) // >= 65520 rounds to infinity
                return static_cast<uint16_t>(sign | 0x7C00u);

            if (absx < 0x38800000u) // below the smallest normal half (2^-14)
            {
                if (absx < 0x33000000u) // below 2^-25 rounds to zero
                    return static_cast<uint16_t>(sign);

                const uint32_t exponent = absx >> 23;
                const uint32_t mantissa = (absx & 0x7FFFFFu) | 0x800000u;
                const uint32_t shift    = 126u - exponent; // 14..24
                uint32_t half           = mantissa >> shift;
                const uint32_t rem      = mantissa & ((1u << shift) - 1u);
                const uint32_t halfway  = 1u << (shift - 1u);
                if (rem > halfway || (rem == halfway && (half & 1u)))
                    ++half;
                return static_cast<uint16_t>(sign | half);
            }

            uint32_t half      = (absx - 0x38000000u) >> 13; // rebias exponent, drop 13 mantissa bits
            const uint32_t rem = absx & 0x1FFFu;
            if (rem > 0x1000u || (rem == 0x1000u && (half & 1u)))
                ++half; // a mantissa carry correctly bumps the exponent
            return static_cast<uint16_t>(sign | half);
        }

        float Saturate(float v)
        {
            return v > 0.0f ? (v < 1.0f ? v : 1.0f) : 0.0f; // NaN -> 0
        }

        float ReadU8(const uint8_t *p) { return static_cast<float>(*p) / 255.0f; }
        float ReadU16(const uint8_t *p)
        {
            uint16_t v;
            std::memcpy(&v, p, sizeof(v));
            return static_cast<float>(v) / 65535.0f;
        }
        float ReadF16(const uint8_t *p)
        {
            uint16_t v;
            std::memcpy(&v, p, sizeof(v));
            return HalfToFloat(v);
        }
        float ReadF32(const uint8_t *p)
        {
            float v;
            std::memcpy(&v, p, sizeof(v));
            return v;
        }

        void WriteU8(uint8_t *p, float v) { *p = static_cast<uint8_t>(Saturate(v) * 255.0f + 0.5f); }
        void WriteU16(uint8_t *p, float v)
        {
            const uint16_t out = static_cast<uint16_t>(Saturate(v) * 65535.0f + 0.5f);
            std::memcpy(p, &out, sizeof(out));
        }
        void WriteF16(uint8_t *p, float v)
        {
            const uint16_t out = FloatToHalf(v);
            std::memcpy(p, &out, sizeof(out));
        }
        void WriteF32(uint8_t *p, float v) { std::memcpy(p, &v, sizeof(v)); }

        using ReadFn  = float (*)(const uint8_t *);
        using WriteFn = void (*)(uint8_t *, float);

        ReadFn SelectReader(PixelType type)
        {
            switch (type)
            {
                case PixelType::UInt8:
                    return &ReadU8;
                case PixelType::UInt16:
                    return &ReadU16;
                case PixelType::Float16:
                    return &ReadF16;
                case PixelType::Float32:
                    return &ReadF32;
            }
            throw std::invalid_argument("Unknown pixel type");
        }

        WriteFn SelectWriter(PixelType type)
        {
            switch (type)
            {
                case PixelType::UInt8:
                    return &WriteU8;
                case PixelType::UInt16:
                    return &WriteU16;
                case PixelType::Float16:
                    return &WriteF16;
                case PixelType::Float32:
                    return &WriteF32;
            }
            throw std::invalid_argument("Unknown pixel type");
        }
    } // namespace

    Bitmap::Bitmap(const std::filesystem::path &filename) { Load(filename); }

    Bitmap::Bitmap(const UVec2 &size, PixelFormat format) : size(size), format(format)
    {
        data = std::make_unique<uint8_t[]>(CalculateLength(size, format));
    }

    Bitmap::Bitmap(std::unique_ptr<uint8_t[]> &&data, const UVec2 &size, PixelFormat format) :
        data(std::move(data)), size(size), format(format)
    {
    }

    Bitmap::Bitmap(const UVec2 &size, uint32_t bytesPerPixel) : size(size)
    {
        SetBytesPerPixel(bytesPerPixel);
        data = std::make_unique<uint8_t[]>(CalculateLength(size, format));
    }

    Bitmap::Bitmap(std::unique_ptr<uint8_t[]> &&data, const UVec2 &size, uint32_t bytesPerPixel) :
        data(std::move(data)), size(size)
    {
        SetBytesPerPixel(bytesPerPixel);
    }

    void Bitmap::Load(const std::filesystem::path &filename)
    {
        const std::string extension = NormalizeExtension(filename.extension().string());
        const auto &registry        = Registry();
        const auto it               = registry.find(extension);
        if (it == registry.end())
        {
            throw std::runtime_error("No bitmap loader registered for '." + extension + "': " + filename.string());
        }
        it->second.first(*this, filename);
    }

    void Bitmap::Write(const std::filesystem::path &filename) const
    {
        const std::string extension = NormalizeExtension(filename.extension().string());
        const auto &registry        = Registry();
        const auto it               = registry.find(extension);
        if (it == registry.end())
        {
            throw std::runtime_error("No bitmap writer registered for '." + extension + "': " + filename.string());
        }
        it->second.second(*this, filename);
    }

    Bitmap Bitmap::Converted(PixelFormat target) const
    {
        if (!data)
        {
            throw std::runtime_error("Cannot convert an empty bitmap");
        }

        Bitmap result(size, target);
        result.filename = filename;

        if (target == format)
        {
            std::memcpy(result.data.get(), data.get(), GetLength());
            return result;
        }

        const uint32_t srcChannels = PixelFormatChannels(format);
        const uint32_t dstChannels = PixelFormatChannels(target);
        const size_t srcBytes      = PixelTypeBytes(PixelFormatType(format));
        const size_t dstBytes      = PixelTypeBytes(PixelFormatType(target));
        const ReadFn read          = SelectReader(PixelFormatType(format));
        const WriteFn write        = SelectWriter(PixelFormatType(target));

        const size_t pixelCount = static_cast<size_t>(size.x) * static_cast<size_t>(size.y);
        const uint8_t *src      = data.get();
        uint8_t *dst            = result.data.get();

        for (size_t i = 0; i < pixelCount; ++i)
        {
            float c[4] = {0.0f, 0.0f, 0.0f, 1.0f};

            for (uint32_t ch = 0; ch < srcChannels; ++ch)
                c[ch] = read(src + (i * srcChannels + ch) * srcBytes);

            if (srcChannels == 1) // gray -> RGB
                c[1] = c[2] = c[0];

            if (dstChannels == 1 && srcChannels >= 3) // color -> gray (Rec.709 luma)
                c[0] = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];

            for (uint32_t ch = 0; ch < dstChannels; ++ch)
                write(dst + (i * dstChannels + ch) * dstBytes, c[ch]);
        }

        return result;
    }

    void Bitmap::SetBytesPerPixel(uint32_t bytesPerPixel)
    {
        switch (bytesPerPixel)
        {
            case 1:
                format = PixelFormat::R8;
                break;
            case 2:
                format = PixelFormat::RG8;
                break;
            case 3:
                format = PixelFormat::RGB8;
                break;
            case 4:
                format = PixelFormat::RGBA8;
                break;
            default:
                throw std::invalid_argument("SetBytesPerPixel only supports 8-bit formats (1-4 bytes); "
                                            "use SetFormat() for 16-bit and float data");
        }
    }

    size_t Bitmap::GetLength() const { return CalculateLength(size, format); }

    size_t Bitmap::CalculateLength(const UVec2 &size, PixelFormat format)
    {
        return static_cast<size_t>(size.x) * static_cast<size_t>(size.y) * PixelFormatBytesPerPixel(format);
    }
} // namespace SF::Engine
