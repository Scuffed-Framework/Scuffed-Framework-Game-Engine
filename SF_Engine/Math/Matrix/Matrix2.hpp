#pragma once
#include <glm/glm.hpp>
#include "../Math.hpp"

namespace SF::Engine
{
    using Matrix2float = glm::mat2;
    using Matrix2double = glm::dmat2;
}

namespace std
{
    template <>
    struct hash<SF::Engine::Matrix2float>
    {
        size_t operator()(const SF::Engine::Matrix2float &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Maths::HashCombine(seed, matrix[0]);
            SF::Engine::Maths::HashCombine(seed, matrix[1]);
            return seed;
        }
    };

    template <>
    struct hash<SF::Engine::Matrix2double>
    {
        size_t operator()(const SF::Engine::Matrix2double &matrix) const noexcept
        {
            size_t seed = 0;
            SF::Engine::Maths::HashCombine(seed, matrix[0]);
            SF::Engine::Maths::HashCombine(seed, matrix[1]);
            return seed;
        }
    };
}