#pragma once
#include <glm/glm.hpp>
#include "../Math.hpp"

namespace SF::Engine
{
    using Mat2 = glm::mat2;
    using DMat2 = glm::dmat2;
}

namespace std
{
    template <>
    struct hash<SF::Engine::Mat2>
    {
        size_t operator()(const SF::Engine::Mat2 &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Mathematics::HashCombine(seed, matrix[0]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[1]);
            return seed;
        }
    };

    template <>
    struct hash<SF::Engine::DMat2>
    {
        size_t operator()(const SF::Engine::DMat2 &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Mathematics::HashCombine(seed, matrix[0]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[1]);
            return seed;
        }
    };
}