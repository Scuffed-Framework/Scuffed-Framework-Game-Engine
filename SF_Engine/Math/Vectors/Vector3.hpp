#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <type_traits>

namespace SF::Engine
{
    using Vec3  = glm::vec3;
    using DVec3 = glm::dvec3;
    using IVec3 = glm::ivec3;
    using UVec3 = glm::uvec3;

    using Ui32Vec3 = UVec3;
    using Ui64Vec3 = glm::u64vec3;
    using Ui16Vec3 = glm::u16vec3;
    using Ui8Vec3  = glm::u8vec3;

    using I32Vec3 = IVec3;
    using I64Vec3 = glm::i64vec3;
    using I16Vec3 = glm::i16vec3;
    using I8Vec3  = glm::i8vec3;

    using TVec3 = glm::tvec3<glm::uint16>;
    using BVec3 = glm::bvec3;

    inline IVec3 operator+(const IVec3 &lhs, const UVec3 &rhs) noexcept
    {
        return lhs + IVec3(static_cast<int>(rhs.x), static_cast<int>(rhs.y), static_cast<int>(rhs.z));
    }

    inline IVec3 operator+(const UVec3 &lhs, const IVec3 &rhs) noexcept
    {
        return IVec3(static_cast<int>(lhs.x), static_cast<int>(lhs.y), static_cast<int>(lhs.z)) + rhs;
    }
} // namespace SF::Engine
