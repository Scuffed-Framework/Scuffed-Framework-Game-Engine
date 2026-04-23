#pragma once

#include <cstdint>
#include <type_traits>
#include <glm/glm.hpp>

namespace SF::Engine
{
    // GLM is better
    using Vector2float = glm::vec2;
    using Vector2double = glm::dvec2;
    using Vector2int = glm::ivec2;
    using Vector2Uint = glm::uvec2;
    using Vector2Ushort = glm::tvec2<glm::uint16>;

    inline Vector2Ushort MakeVector2Ushort(glm::uint16 x, glm::uint16 y) noexcept
    {
        return Vector2Ushort(x, y);
    }

    inline glm::ivec2 operator+(const glm::ivec2 &lhs, const Vector2Uint &rhs) noexcept
    {
        return lhs + glm::ivec2(static_cast<int>(rhs.x), static_cast<int>(rhs.y));
    }

    inline glm::ivec2 operator+(const Vector2Uint &lhs, const glm::ivec2 &rhs) noexcept
    {
        return glm::ivec2(static_cast<int>(lhs.x), static_cast<int>(lhs.y)) + rhs;
    }

    inline glm::ivec2 operator-(const glm::ivec2 &lhs, const Vector2Uint &rhs) noexcept
    {
        return lhs - glm::ivec2(static_cast<int>(rhs.x), static_cast<int>(rhs.y));
    }

    inline glm::ivec2 operator-(const Vector2Uint &lhs, const glm::ivec2 &rhs) noexcept
    {
        return glm::ivec2(static_cast<int>(lhs.x), static_cast<int>(lhs.y)) - rhs;
    }
}