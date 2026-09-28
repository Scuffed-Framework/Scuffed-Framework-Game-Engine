#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <type_traits>

namespace SF::Engine
{
    using Vec4  = glm::vec4;
    using DVec4 = glm::dvec4;
    using IVec4 = glm::ivec4;
    using UVec4 = glm::uvec4;
    using TVec4 = glm::tvec4<glm::uint16>;
    using BVec4 = glm::bvec4;

    using Ui32Vec4 = UVec4;
    using Ui64Vec4 = glm::u64vec4;
    using Ui16Vec4 = glm::u16vec4;
    using Ui8Vec4  = glm::u8vec4;

    using I32Vec4 = IVec4;
    using I64Vec4 = glm::i64vec4;
    using I16Vec4 = glm::i16vec4;
    using I8Vec4  = glm::i8vec4;

    inline IVec4 operator+(const IVec4 &lhs, const UVec4 &rhs) noexcept
    {
        return lhs + IVec4(static_cast<int>(rhs.x), static_cast<int>(rhs.y), static_cast<int>(rhs.z),
                           static_cast<int>(rhs.w));
    }

    inline IVec4 operator+(const UVec4 &lhs, const IVec4 &rhs) noexcept
    {
        return IVec4(static_cast<int>(lhs.x), static_cast<int>(lhs.y), static_cast<int>(lhs.z),
                     static_cast<int>(lhs.w)) +
               rhs;
    }

    inline IVec4 operator-(const IVec4 &lhs, const UVec4 &rhs) noexcept
    {
        return lhs - IVec4(static_cast<int>(rhs.x), static_cast<int>(rhs.y), static_cast<int>(rhs.z),
                           static_cast<int>(rhs.w));
    }

    inline IVec4 operator-(const UVec4 &lhs, const IVec4 &rhs) noexcept
    {
        return IVec4(static_cast<int>(lhs.x), static_cast<int>(lhs.y), static_cast<int>(lhs.z),
                     static_cast<int>(lhs.w)) -
               rhs;
    }
} // namespace SF::Engine
