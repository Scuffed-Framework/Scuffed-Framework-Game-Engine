#pragma once
#include <glm/glm.hpp>
#include "../Math.hpp"

namespace SF::Engine
{
    using Matrix4float = glm::mat4;
    using Matrix4double = glm::dmat4;
}

namespace std
{
    template <>
    struct hash<SF::Engine::Matrix4float>
    {
        size_t operator()(const SF::Engine::Matrix4float &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Maths::HashCombine(seed, matrix[0]);
            SF::Engine::Maths::HashCombine(seed, matrix[1]);
            SF::Engine::Maths::HashCombine(seed, matrix[2]);
            SF::Engine::Maths::HashCombine(seed, matrix[3]);
            return seed;
        }
    };

    template <>
    struct hash<SF::Engine::Matrix4double>
    {
        size_t operator()(const SF::Engine::Matrix4double &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Maths::HashCombine(seed, matrix[0]);
            SF::Engine::Maths::HashCombine(seed, matrix[1]);
            SF::Engine::Maths::HashCombine(seed, matrix[2]);
            SF::Engine::Maths::HashCombine(seed, matrix[3]);
            return seed;
        }
    };
}