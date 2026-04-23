#pragma once

#include <cstdint>
#include <type_traits>
#include <glm/glm.hpp>

namespace SF::Engine
{
    using Vector3float = glm::vec3;
    using Vector3double = glm::dvec3;
    using Vector3int = glm::ivec3;
    using Vector3Uint = glm::uvec3;

    inline glm::ivec3 operator+(const glm::ivec3 &lhs, const Vector3Uint &rhs) noexcept
    {
        return lhs + glm::ivec3(static_cast<int>(rhs.x), static_cast<int>(rhs.y), static_cast<int>(rhs.z));
    }

    inline glm::ivec3 operator+(const Vector3Uint &lhs, const glm::ivec3 &rhs) noexcept
    {
        return glm::ivec3(static_cast<int>(lhs.x), static_cast<int>(lhs.y), static_cast<int>(lhs.z)) + rhs;
    }
}
