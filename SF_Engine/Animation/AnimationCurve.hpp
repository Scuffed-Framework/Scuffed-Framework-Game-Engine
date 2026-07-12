#pragma once
#include <Math/KVP.hpp>
#include <vector>

namespace SF::Engine
{
    class AnimationCurve
    {
    public:
        // Properties
        std::vector<KeyValuePair<float, float>> keyframes;
        const int length = static_cast<int>(keyframes.size());

        KeyValuePair<float, float> AddKeyframe(float time, float value)
        {
            KeyValuePair<float, float> kvp(time, value);
            keyframes.push_back(kvp);
            return kvp;
        }
    };
} // namespace SF::Engine