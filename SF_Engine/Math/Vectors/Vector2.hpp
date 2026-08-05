#pragma once

#include <cstdint>
#include <type_traits>
#include <glm/glm.hpp>

namespace SF::Engine
{
    // GLM is better
    using Vec2 = glm::vec2;
    using DVec2 = glm::dvec2;
    using IVec2 = glm::ivec2;
    using UVec2 = glm::uvec2;
    using TVec2 = glm::tvec2<glm::uint16>;
    using BVec2 = glm::bvec2;

    inline TVec2 MakeTVec2(glm::uint16 x, glm::uint16 y) noexcept
    {
        return TVec2(x, y);
    }

    inline glm::ivec2 operator+(const glm::ivec2 &lhs, const UVec2 &rhs) noexcept
    {
        return lhs + glm::ivec2(static_cast<int>(rhs.x), static_cast<int>(rhs.y));
    }

    inline glm::ivec2 operator+(const UVec2 &lhs, const glm::ivec2 &rhs) noexcept
    {
        return glm::ivec2(static_cast<int>(lhs.x), static_cast<int>(lhs.y)) + rhs;
    }

    inline glm::ivec2 operator-(const glm::ivec2 &lhs, const UVec2 &rhs) noexcept
    {
        return lhs - glm::ivec2(static_cast<int>(rhs.x), static_cast<int>(rhs.y));
    }

    inline glm::ivec2 operator-(const UVec2 &lhs, const glm::ivec2 &rhs) noexcept
    {
        return glm::ivec2(static_cast<int>(lhs.x), static_cast<int>(lhs.y)) - rhs;
    }
}