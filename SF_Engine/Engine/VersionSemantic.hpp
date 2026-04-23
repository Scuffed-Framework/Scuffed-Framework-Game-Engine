#include "Version.hpp"

namespace SF::Engine
{
    class Version
    {
    public:
        constexpr Version(uint16_t major = Engine_VERSION_MAJOR,
                          uint8_t minor = Engine_VERSION_MINOR,
                          uint16_t patch = Engine_VERSION_PATCH) noexcept
            : major(major), minor(minor), patch(patch)
        {
        }

        [[nodiscard]] constexpr bool operator==(const Version &other) const noexcept
        {
            return major == other.major && minor == other.minor && patch == other.patch;
        }

        [[nodiscard]] constexpr bool operator!=(const Version &other) const noexcept
        {
            return !(*this == other);
        }

        [[nodiscard]] constexpr bool operator<(const Version &other) const noexcept
        {
            if (major != other.major)
                return major < other.major;
            if (minor != other.minor)
                return minor < other.minor;
            return patch < other.patch;
        }

        [[nodiscard]] constexpr std::string_view ToString() const noexcept
        {
            // Simple compile-time string construction is tricky; return prebuilt string
            return Engine_VERSION;
        }

        uint16_t major;
        uint8_t minor;
        uint16_t patch;
    };
}