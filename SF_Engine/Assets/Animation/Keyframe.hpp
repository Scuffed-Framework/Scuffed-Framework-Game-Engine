#pragma once
#include <Math/Time/Time.hpp>
#include <map>
#include <string>
#include <utility>
#include "Joint/AnimJoint.hpp"

namespace SF::Engine::Animation
{
    using namespace std;
    struct Keyframe
    {
        Keyframe() = default;
        Keyframe(const ApplicationTime TimeStamp, map<string, JointTransform> Pose) :
            TimeStamp(TimeStamp), Pose(std::move(Pose))
        {
        }

        void AddJointTransform(const string &jointNameId, const Mat4 &jointLocalTransform)
        {
            Pose.emplace(jointNameId, jointLocalTransform);
        }

        [[nodiscard]] const ApplicationTime &GetTimeStamp() const { return TimeStamp; }

        [[nodiscard]] const map<string, JointTransform> &GetPose() const { return Pose; }

        bool operator==(const Keyframe &other) const { return TimeStamp == other.TimeStamp && Pose == other.Pose; }

    private:
        ApplicationTime TimeStamp;
        map<string, JointTransform> Pose;
    };
} // namespace SF::Engine::Animation
