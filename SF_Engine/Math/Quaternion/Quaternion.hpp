#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#include <glm/gtc/quaternion.hpp>
#pragma GCC diagnostic pop

#include "../Math.hpp"

namespace SF::Engine
{
    using Quaternion = glm::quat;
}
namespace std
{
    template<>
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
} // namespace std
