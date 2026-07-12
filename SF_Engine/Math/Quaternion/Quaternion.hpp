#pragma once
#include <glm/gtc/quaternion.hpp>
#include "../Math.hpp"

namespace SF::Engine
{
    using Quaternion = glm::quat;
}
namespace std
{
    template <>
    struct hash<SF::Engine::Quaternion>
    {
        size_t operator()(const SF::Engine::Quaternion &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Mathematics::HashCombine(seed, matrix[0]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[1]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[2]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[3]);
            return seed;
        }
    };
}