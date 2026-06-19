#pragma once
#include <cstdint>
#include <functional>

namespace SF::Engine
{
    // Opaque 64-bit handle identical in layout to the engine's Handle type.
    // Platform implementations store a native pointer/ID cast to uint64_t.
    struct Handle
    {
        uint64_t value = 0;

        bool IsValid() const noexcept { return value != 0; }

        bool operator==(const Handle &o) const noexcept { return value == o.value; }
        bool operator!=(const Handle &o) const noexcept { return value != o.value; }
        bool operator<(const Handle &o) const noexcept { return value < o.value; }

        static Handle Invalid() noexcept { return {0}; }
    };
}

// Allow Handle as a std::unordered_map key
namespace std
{
    template <>
    struct hash<SF::Engine::Handle>
    {
        std::size_t operator()(const SF::Engine::Handle &h) const noexcept
        {
            return std::hash<uint64_t>{}(h.value);
        }
    };
}
