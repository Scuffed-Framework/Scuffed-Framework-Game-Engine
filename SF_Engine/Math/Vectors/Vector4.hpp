#pragma once

#include <cstdint>
#include <type_traits>
#include <glm/glm.hpp>

namespace SF::Engine
{
    using Vector4float = glm::vec4;
    using Vector4double = glm::dvec4;
    using Vector4int = glm::ivec4;
    using Vector4Uint = glm::uvec4;
    using Vector4Ushort = glm::tvec4<glm::uint16>;

    inline glm::ivec4 operator+(const glm::ivec4 &lhs, const Vector4Uint &rhs) noexcept
    {
        return lhs + glm::ivec4(static_cast<int>(rhs.x), static_cast<int>(rhs.y), static_cast<int>(rhs.z), static_cast<int>(rhs.w));
    }

    inline glm::ivec4 operator+(const Vector4Uint &lhs, const glm::ivec4 &rhs) noexcept
    {
        return glm::ivec4(static_cast<int>(lhs.x), static_cast<int>(lhs.y), static_cast<int>(lhs.z), static_cast<int>(lhs.w)) + rhs;
    }

    inline glm::ivec4 operator-(const glm::ivec4 &lhs, const Vector4Uint &rhs) noexcept
    {
        return lhs - glm::ivec4(static_cast<int>(rhs.x), static_cast<int>(rhs.y), static_cast<int>(rhs.z), static_cast<int>(rhs.w));
    }

    inline glm::ivec4 operator-(const Vector4Uint &lhs, const glm::ivec4 &rhs) noexcept
    {
        return glm::ivec4(static_cast<int>(lhs.x), static_cast<int>(lhs.y), static_cast<int>(lhs.z), static_cast<int>(lhs.w)) - rhs;
    }
}