#pragma once

#include <cstdint>
#include <type_traits>
#include <glm/glm.hpp>

namespace SF::Engine
{
    using Vec3 = glm::vec3;
    using DVec3 = glm::dvec3;
    using IVec3 = glm::ivec3;
    using UVec3 = glm::uvec3;
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
}
