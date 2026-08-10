#pragma once

#include <cstdint>
#include <type_traits>
#include <glm/glm.hpp>

namespace SF::Engine
{
    using Vec4 = glm::vec4;
    using DVec4 = glm::dvec4;
    using IVec4 = glm::ivec4;
    using UVec4 = glm::uvec4;
    using TVec4 = glm::tvec4<glm::uint16>;
    using BVec4 = glm::bvec4;

    inline IVec4 operator+(const IVec4 &lhs, const UVec4 &rhs) noexcept
    {
        return lhs + IVec4(static_cast<int>(rhs.x), static_cast<int>(rhs.y), static_cast<int>(rhs.z), static_cast<int>(rhs.w));
    }

    inline IVec4 operator+(const UVec4 &lhs, const IVec4 &rhs) noexcept
    {
        return IVec4(static_cast<int>(lhs.x), static_cast<int>(lhs.y), static_cast<int>(lhs.z), static_cast<int>(lhs.w)) + rhs;
    }

    inline IVec4 operator-(const IVec4 &lhs, const UVec4 &rhs) noexcept
    {
        return lhs - IVec4(static_cast<int>(rhs.x), static_cast<int>(rhs.y), static_cast<int>(rhs.z), static_cast<int>(rhs.w));
    }

    inline IVec4 operator-(const UVec4 &lhs, const IVec4 &rhs) noexcept
    {
        return IVec4(static_cast<int>(lhs.x), static_cast<int>(lhs.y), static_cast<int>(lhs.z), static_cast<int>(lhs.w)) - rhs;
    }
}