#pragma once
#include <Math/Time/Time.hpp>
#include "Keyframe.hpp"

namespace SF::Engine::Animation
{
    using namespace std;
    class Animation
    {
        DurationAsMilliseconds length;
        vector<Keyframe> keyframes;

    public:
        Animation(const DurationAsMilliseconds &length, std::vector<Keyframe> keyframes) :
            length(length), keyframes(std::move(keyframes))
        {
        }

        [[nodiscard]] const DurationAsMilliseconds &GetLength() const { return length; }
        [[nodiscard]] const std::vector<Keyframe> &GetKeyframes() const { return keyframes; }
    };
} // namespace SF::Engine::Animation
