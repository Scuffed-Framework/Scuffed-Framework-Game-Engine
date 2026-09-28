#pragma once
#include <Math/Time/Time.hpp>
#include "Keyframe.hpp"

namespace SF::Engine::Animation
{
    using namespace std;
    class Animation
    {
        ApplicationTime length;
        vector<Keyframe> keyframes;

    public:
        Animation(const ApplicationTime &length, std::vector<Keyframe> keyframes) :
            length(length), keyframes(std::move(keyframes))
        {
        }

        const ApplicationTime &GetLength() const { return length; }
        const std::vector<Keyframe> &GetKeyframes() const { return keyframes; }
    };
} // namespace SF::Engine::Animation
