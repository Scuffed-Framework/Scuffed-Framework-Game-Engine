#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <type_traits>

namespace SF::Engine
{
    // GLM is better
    using Vec2  = glm::vec2;
    using DVec2 = glm::dvec2;
    using IVec2 = glm::ivec2;
    using UVec2 = glm::uvec2;
    using TVec2 = glm::tvec2<glm::uint16>;
    using BVec2 = glm::bvec2;

    using Ui32vec2 = UVec2;
    using Ui64vec2 = glm::u64vec2;
    using Ui16vec2 = glm::u16vec2;
    using Ui8vec2  = glm::u8vec2;

    using I32vec2 = IVec2;
    using I64vec2 = glm::i64vec2;
    using I16vec2 = glm::i16vec2;
    using I8vec2  = glm::i8vec2;

    inline TVec2 MakeTVec2(glm::uint16 x, glm::uint16 y) noexcept { return TVec2(x, y); }

    inline IVec2 operator+(const IVec2 &lhs, const UVec2 &rhs) noexcept
    {
        return lhs + IVec2(static_cast<int>(rhs.x), static_cast<int>(rhs.y));
    }

    inline IVec2 operator+(const UVec2 &lhs, const IVec2 &rhs) noexcept
    {
        return IVec2(static_cast<int>(lhs.x), static_cast<int>(lhs.y)) + rhs;
    }

    inline IVec2 operator-(const IVec2 &lhs, const UVec2 &rhs) noexcept
    {
        return lhs - IVec2(static_cast<int>(rhs.x), static_cast<int>(rhs.y));
    }

    inline IVec2 operator-(const UVec2 &lhs, const IVec2 &rhs) noexcept
    {
        return IVec2(static_cast<int>(lhs.x), static_cast<int>(lhs.y)) - rhs;
    }
} // namespace SF::Engine
