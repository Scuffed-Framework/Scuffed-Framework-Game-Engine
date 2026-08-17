#pragma once //  todo: move this into UtilityClasses/ resolved 8/15/28 
#include <array>
#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>
#include <random>
#include <functional>
#include <cstring>
#include <algorithm>
namespace SF::Engine
{
    class UUID
    {
    private:
        std::array<uint8_t, 16> data;

        // Thread-safe random number generator (non-constexpr)
        static std::mt19937 &get_random_engine()
        {
            static thread_local std::mt19937 engine(std::random_device{}() ^ static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&engine)));
            return engine;
        }

        static uint8_t random_byte()
        {
            static std::uniform_int_distribution<uint16_t> dist(0, 255);
            return static_cast<uint8_t>(dist(get_random_engine()));
        }

        // Helper function to parse hex char (constexpr)
        static constexpr uint8_t hex_char_to_byte(char c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F')
                return 10 + (c - 'A');
            return 0; // Invalid, but we can't throw in constexpr
        }

    public:
        // Default constructor (creates null UUID) - constexpr
        constexpr UUID() : data{0} {}

        // Constexpr constructor from array - needed for literal type
        constexpr UUID(const std::array<uint8_t, 16> &arr) : data(arr) {}

        // Constexpr constructor from initializer list
        constexpr UUID(std::initializer_list<uint8_t> init) : data{0}
        {
            size_t i = 0;
            for (auto val : init)
            {
                if (i < 16)
                    data[i++] = val;
            }
        }

        // Constexpr copy constructor
        constexpr UUID(const UUID &other) = default;

        // Constexpr assignment
        constexpr UUID &operator=(const UUID &other) = default;

        // Generate a new UUID (version 4, variant 1) - NOT constexpr
        static UUID Generate()
        {
            UUID uuid;
            auto &engine = get_random_engine();

            // Fill with random bytes
            for (auto &byte : uuid.data)
            {
                byte = random_byte();
            }

            // Set version to 4 (random)
            uuid.data[6] = (uuid.data[6] & 0x0F) | 0x40;

            // Set variant to RFC 4122
            uuid.data[8] = (uuid.data[8] & 0x3F) | 0x80;

            return uuid;
        }

        // Create from existing bytes - NOT constexpr
        static UUID FromBytes(const uint8_t bytes[16])
        {
            UUID uuid;
            std::memcpy(uuid.data.data(), bytes, 16);
            return uuid;
        }

        // Create from string (can throw std::invalid_argument) - NOT constexpr
        static UUID FromString(const std::string &str)
        {
            if (str.length() != 36)
            {
                throw std::invalid_argument("Invalid UUID string length");
            }

            std::array<uint8_t, 16> arr{};
            size_t idx = 0;

            for (size_t i = 0; i < 36; ++i)
            {
                if (str[i] == '-')
                    continue;

                if (i >= 35 || !isxdigit(static_cast<unsigned char>(str[i])) ||
                    !isxdigit(static_cast<unsigned char>(str[i + 1])))
                {
                    throw std::invalid_argument("Invalid UUID format");
                }

                std::string byteStr = str.substr(i, 2);
                arr[idx++] = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
                i++; // Skip second hex digit
            }

            return UUID(arr);
        }

        // Constexpr version for compile-time string parsing
        template <size_t N>
        static constexpr UUID FromStringConstexpr(const char (&str)[N])
        {
            static_assert(N == 37, "UUID string must be exactly 36 characters plus null terminator");

            std::array<uint8_t, 16> arr{};
            size_t idx = 0;

            for (size_t i = 0; i < 36; ++i)
            {
                if (str[i] == '-')
                    continue;

                // Check bounds and validate hex chars
                if (i >= 35 ||
                    !((str[i] >= '0' && str[i] <= '9') ||
                      (str[i] >= 'a' && str[i] <= 'f') ||
                      (str[i] >= 'A' && str[i] <= 'F')) ||
                    !((str[i + 1] >= '0' && str[i + 1] <= '9') ||
                      (str[i + 1] >= 'a' && str[i + 1] <= 'f') ||
                      (str[i + 1] >= 'A' && str[i + 1] <= 'F')))
                {
                    return UUID(); // Return null UUID on error
                }

                uint8_t high = hex_char_to_byte(str[i]);
                uint8_t low = hex_char_to_byte(str[i + 1]);
                arr[idx++] = (high << 4) | low;
                i++; // Skip second hex digit
            }

            return UUID(arr);
        }

        // Alternative: Use a raw pointer with length check
        static constexpr UUID FromStringConstexpr(const char *str)
        {
            // We can't verify length at compile-time with raw pointer
            // So we'll just parse what we get
            std::array<uint8_t, 16> arr{};
            size_t idx = 0;

            for (size_t i = 0; i < 36 && str[i] != '\0'; ++i)
            {
                if (str[i] == '-')
                    continue;

                // Check if we have enough characters
                if (i >= 35 || str[i + 1] == '\0')
                {
                    return UUID();
                }

                uint8_t high = hex_char_to_byte(str[i]);
                uint8_t low = hex_char_to_byte(str[i + 1]);
                arr[idx++] = (high << 4) | low;
                i++; // Skip second hex digit
            }

            if (idx != 16)
            {
                return UUID(); // Didn't parse enough bytes
            }

            return UUID(arr);
        }

        // Comparison operators - constexpr
        constexpr bool operator==(const UUID &other) const
        {
            for (size_t i = 0; i < 16; ++i)
            {
                if (data[i] != other.data[i])
                    return false;
            }
            return true;
        }

        constexpr bool operator!=(const UUID &other) const
        {
            return !(*this == other);
        }

        constexpr bool operator<(const UUID &other) const
        {
            for (size_t i = 0; i < 16; ++i)
            {
                if (data[i] != other.data[i])
                {
                    return data[i] < other.data[i];
                }
            }
            return false;
        }

        // Check if null UUID (all zeros) - constexpr
        constexpr bool IsNull() const
        {
            for (auto byte : data)
            {
                if (byte != 0)
                    return false;
            }
            return true;
        }

        // Convert to string (format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)
        std::string ToString() const
        {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');

            // First 4 bytes
            for (int i = 0; i < 4; ++i)
            {
                ss << std::setw(2) << static_cast<int>(data[i]);
            }
            ss << '-';

            // Next 2 bytes
            for (int i = 4; i < 6; ++i)
            {
                ss << std::setw(2) << static_cast<int>(data[i]);
            }
            ss << '-';

            // Next 2 bytes
            for (int i = 6; i < 8; ++i)
            {
                ss << std::setw(2) << static_cast<int>(data[i]);
            }
            ss << '-';

            // Next 2 bytes
            for (int i = 8; i < 10; ++i)
            {
                ss << std::setw(2) << static_cast<int>(data[i]);
            }
            ss << '-';

            // Last 6 bytes
            for (int i = 10; i < 16; ++i)
            {
                ss << std::setw(2) << static_cast<int>(data[i]);
            }

            return ss.str();
        }

        // Upper-case string version
        std::string ToUpperString() const
        {
            auto str = ToString();
            std::transform(str.begin(), str.end(), str.begin(), ::toupper);
            return str;
        }

        // Access raw bytes - constexpr
        constexpr const uint8_t *Bytes() const { return data.data(); }
        constexpr size_t Size() const { return data.size(); }

        // For use in std::unordered_map
        struct Hash
        {
            size_t operator()(const UUID &uuid) const
            {
                // Use std::hash on the underlying bytes
                size_t hash = 0;
                const uint64_t *ptr = reinterpret_cast<const uint64_t *>(uuid.data.data());
                std::hash<uint64_t> hasher;
                hash ^= hasher(ptr[0]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                hash ^= hasher(ptr[1]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                return hash;
            }
        };
    };

    // User-defined literal for compile-time UUIDs - constexpr
    consteval UUID operator"" _uuid(const char *str, size_t len)
    {
        if (len != 36)
        {
            // In compile-time context, return null UUID for invalid length
            return UUID();
        }

        // Use the constexpr parsing function
        return UUID::FromStringConstexpr(str);
    }

    // Null UUID constant - constexpr
    constexpr UUID NullUUID = UUID();

    // Predefined UUIDs (compile-time)
    constexpr UUID UUID_Zero = UUID();
    constexpr UUID UUID_Max = UUID({0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF});

    // Common/well-known UUIDs
    constexpr UUID UUID_Namespace_DNS = "6ba7b810-9dad-11d1-80b4-00c04fd430c8"_uuid;
    constexpr UUID UUID_Namespace_URL = "6ba7b811-9dad-11d1-80b4-00c04fd430c8"_uuid;
    constexpr UUID UUID_Namespace_OID = "6ba7b812-9dad-11d1-80b4-00c04fd430c8"_uuid;
    constexpr UUID UUID_Namespace_X500 = "6ba7b814-9dad-11d1-80b4-00c04fd430c8"_uuid;
}
namespace std
{
    template <>
    struct hash<SF::Engine::UUID>
    {
        size_t operator()(const SF::Engine::UUID &uuid) const
        {
            // Use the built-in Hash struct from UUID
            return SF::Engine::UUID::Hash{}(uuid);
        }
    };
}
