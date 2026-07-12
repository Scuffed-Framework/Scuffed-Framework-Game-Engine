#pragma once
#include <Math/Time/Time.hpp>
#include <map>
#include <string>
#include "Joint/AnimJoint.hpp"

namespace SF::Engine::Animation
{
    struct Keyframe
    {
        Keyframe() = default;
        Keyframe(const ApplicationTime TimeStamp, std::map<std::string, JointTransform> Pose) : TimeStamp(TimeStamp), Pose(Pose) {}

        void AddJointTransform(const std::string &jointNameId, const Matrix4float &jointLocalTransform) { Pose.emplace(jointNameId, jointLocalTransform); }

        const ApplicationTime &GetTimeStamp() const { return TimeStamp; }

        const std::map<std::string, JointTransform> &GetPose() const { return Pose; }

    private:
        ApplicationTime TimeStamp;
        std::map<std::string, JointTransform> Pose;
    };
}
