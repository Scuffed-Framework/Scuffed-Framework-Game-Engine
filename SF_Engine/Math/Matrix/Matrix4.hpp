#pragma once
#include <glm/glm.hpp>
#include "../Math.hpp"

namespace SF::Engine
{
    using Mat4 = glm::mat4;
    using DMat4 = glm::dmat4;
}

namespace std
{
    template <>
    struct hash<SF::Engine::Mat4>
    {
        size_t operator()(const SF::Engine::Mat4 &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Mathematics::HashCombine(seed, matrix[0]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[1]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[2]);
            SF::Engine::Mathematics::HashCombine(seed, matrix[3]);
            return seed;
        }
    };

    template <>
    struct hash<SF::Engine::DMat4>
    {
        size_t operator()(const SF::Engine::DMat4 &matrix) const noexcept
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