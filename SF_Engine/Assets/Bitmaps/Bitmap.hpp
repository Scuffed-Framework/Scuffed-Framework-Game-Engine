#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include <Math/Vectors/Vector.hpp>

namespace SF::Engine
{
    enum class PixelType : uint8_t
    {
        UInt8   = 0, // normalized [0,1]
        UInt16  = 1, // normalized [0,1]
        Float16 = 2, // IEEE half, unbounded
        Float32 = 3  // IEEE single, unbounded
    };

    enum class PixelFormat : uint8_t
    {
        R8,
        RG8,
        RGB8,
        RGBA8,
        R16,
        RG16,
        RGB16,
        RGBA16,
        R16F,
        RG16F,
        RGB16F,
        RGBA16F,
        R32F,
        RG32F,
        RGB32F,
        RGBA32F
    };

    constexpr PixelFormat MakePixelFormat(PixelType type, uint32_t channels) noexcept
    {
        return static_cast<PixelFormat>(static_cast<uint32_t>(type) * 4u + (channels - 1u));
    }

    constexpr PixelType PixelFormatType(PixelFormat format) noexcept
    {
        return static_cast<PixelType>(static_cast<uint32_t>(format) / 4u);
    }

    constexpr uint32_t PixelFormatChannels(PixelFormat format) noexcept
    {
        return static_cast<uint32_t>(format) % 4u + 1u;
    }

    constexpr uint32_t PixelTypeBytes(PixelType type) noexcept
    {
        switch (type)
        {
            case PixelType::UInt8:
                return 1;
            case PixelType::UInt16:
            case PixelType::Float16:
                return 2;
            case PixelType::Float32:
                return 4;
        }
        return 0;
    }

    constexpr uint32_t PixelFormatBytesPerPixel(PixelFormat format) noexcept
    {
        return PixelFormatChannels(format) * PixelTypeBytes(PixelFormatType(format));
    }

    constexpr bool IsFloatFormat(PixelFormat format) noexcept
    {
        const PixelType type = PixelFormatType(format);
        return type == PixelType::Float16 || type == PixelType::Float32;
    }

    template<typename Base>
    class BitmapFactory
    {
    public:
        using TLoadMethod  = std::function<void(Base &, const std::filesystem::path &)>;
        using TWriteMethod = std::function<void(const Base &, const std::filesystem::path &)>;
        using TRegistryMap = std::unordered_map<std::string, std::pair<TLoadMethod, TWriteMethod>>;

        virtual ~BitmapFactory() = default;

        static TRegistryMap &Registry()
        {
            static TRegistryMap impl;
            return impl;
        }

        // Lowercases and strips a leading '.', so ".PNG", "PNG" and "png" all map to "png".
        static std::string NormalizeExtension(std::string extension)
        {
            if (!extension.empty() && extension.front() == '.')
                extension.erase(extension.begin());
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return extension;
        }

        template<typename T>
        class Registrar
        {
        protected:
            template<typename... Args>
            static bool Register(Args &&...names)
            {
                const TLoadMethod load = [](Base &bitmap, const std::filesystem::path &path) { T::Load(bitmap, path); };
                const TWriteMethod write = [](const Base &bitmap, const std::filesystem::path &path)
                { T::Write(bitmap, path); };

                (BitmapFactory::Registry().insert_or_assign(BitmapFactory::NormalizeExtension(std::string(names)),
                                                            std::make_pair(load, write)),
                 ...);
                return true;
            }
        };
    };

    class Bitmap : public BitmapFactory<Bitmap>
    {
    public:
        Bitmap() = default;
        explicit Bitmap(const std::filesystem::path &filename);
        explicit Bitmap(const UVec2 &size, PixelFormat format = PixelFormat::RGBA8);
        Bitmap(std::unique_ptr<uint8_t[]> &&data, const UVec2 &size, PixelFormat format = PixelFormat::RGBA8);

        // Legacy 8-bit constructors: bytesPerPixel 1/2/3/4 -> R8/RG8/RGB8/RGBA8.
        Bitmap(const UVec2 &size, uint32_t bytesPerPixel);
        Bitmap(std::unique_ptr<uint8_t[]> &&data, const UVec2 &size, uint32_t bytesPerPixel);

        ~Bitmap() = default;

        // User-declared destructor suppresses implicit moves, so declare them explicitly.
        Bitmap(Bitmap &&) noexcept            = default;
        Bitmap &operator=(Bitmap &&) noexcept = default;
        Bitmap(const Bitmap &)                = delete;
        Bitmap &operator=(const Bitmap &)     = delete;

        // Dispatches on the file extension through the registry.
        void Load(const std::filesystem::path &filename);
        void Write(const std::filesystem::path &filename) const;

        // Returns a copy converted to another pixel format (same size).
        // Integer formats are normalized to [0,1] and clamped when written; float formats are not clamped.
        // R expands to gray (R,R,R), RG expands to (R,G,0), missing alpha becomes 1,
        // and RGB(A) -> R uses Rec.709 luma. Converting to the same format makes a plain copy.
        [[nodiscard]] Bitmap Converted(PixelFormat target) const;

        explicit operator bool() const noexcept { return data != nullptr; }

        [[nodiscard]] size_t GetLength() const;

        [[nodiscard]] const std::filesystem::path &GetFilename() const { return filename; }
        void SetFilename(const std::filesystem::path &filename) { this->filename = filename; }

        [[nodiscard]] const std::unique_ptr<uint8_t[]> &GetData() const { return data; }
        std::unique_ptr<uint8_t[]> &GetData() { return data; }
        void SetData(std::unique_ptr<uint8_t[]> &&data) { this->data = std::move(data); }

        // Typed views of the pixel buffer (caller must pass a T matching the format's channel type).
        template<typename T>
        [[nodiscard]] const T *GetDataAs() const
        {
            return reinterpret_cast<const T *>(data.get());
        }
        template<typename T>
        [[nodiscard]] T *GetDataAs()
        {
            return reinterpret_cast<T *>(data.get());
        }

        [[nodiscard]] const UVec2 &GetSize() const { return size; }
        void SetSize(const UVec2 &size) { this->size = size; }

        [[nodiscard]] PixelFormat GetFormat() const { return format; }
        // Relabels the existing buffer; it does NOT convert pixel data. Use Converted() for that.
        void SetFormat(PixelFormat format) { this->format = format; }

        [[nodiscard]] PixelType GetPixelType() const { return PixelFormatType(format); }
        [[nodiscard]] uint32_t GetChannelCount() const { return PixelFormatChannels(format); }
        [[nodiscard]] bool IsFloat() const { return IsFloatFormat(format); }

        [[nodiscard]] uint32_t GetBytesPerPixel() const { return PixelFormatBytesPerPixel(format); }
        // Legacy: sets an 8-bit format with the given channel count (1..4). Throws for anything else.
        void SetBytesPerPixel(uint32_t bytesPerPixel);

    private:
        static size_t CalculateLength(const UVec2 &size, PixelFormat format);

        std::filesystem::path filename;
        std::unique_ptr<uint8_t[]> data;
        UVec2 size;
        PixelFormat format = PixelFormat::RGBA8;
    };
} // namespace SF::Engine
