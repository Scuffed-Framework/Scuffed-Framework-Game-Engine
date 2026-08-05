#pragma once
#include <glm/glm.hpp>
#include "../Math.hpp"

namespace SF::Engine
{
    using Mat3 = glm::mat3;
    using DMat3 = glm::dmat3;
}

namespace std
{
    template <>
    struct hash<SF::Engine::Mat3>
    {
        size_t operator()(const SF::Engine::Mat3 &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Mathematics::HashCombine(seed, matrix[0]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[1]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[2]);
            return seed;
        }
    };

    template <>
    struct hash<SF::Engine::DMat3>
    {
        size_t operator()(const SF::Engine::DMat3 &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Mathematics::HashCombine(seed, matrix[0]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[1]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[2]);
            return seed;
        }
    };
}