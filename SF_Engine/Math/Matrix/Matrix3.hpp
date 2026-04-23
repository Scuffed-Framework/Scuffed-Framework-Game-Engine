#pragma once
#include <glm/glm.hpp>
#include "../Math.hpp"

namespace SF::Engine
{
    using Matrix3float = glm::mat3;
    using Matrix3double = glm::dmat3;
}

namespace std
{
    template <>
    struct hash<SF::Engine::Matrix3float>
    {
        size_t operator()(const SF::Engine::Matrix3float &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Maths::HashCombine(seed, matrix[0]);
            SF::Engine::Maths::HashCombine(seed, matrix[1]);
            SF::Engine::Maths::HashCombine(seed, matrix[2]);
            return seed;
        }
    };

    template <>
    struct hash<SF::Engine::Matrix3double>
    {
        size_t operator()(const SF::Engine::Matrix3double &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Maths::HashCombine(seed, matrix[0]);
            SF::Engine::Maths::HashCombine(seed, matrix[1]);
            SF::Engine::Maths::HashCombine(seed, matrix[2]);
            return seed;
        }
    };
}