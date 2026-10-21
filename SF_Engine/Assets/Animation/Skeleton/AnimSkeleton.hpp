#pragma once

#include <Animation/Joint/AnimJoint.hpp>

namespace SF::Engine::Animation
{
    class Skeleton
    {
        SF_RTTI_BASE(Skeleton)
    public:
        Skeleton(std::vector<std::string> boneOrder, const Mat4 &correction) : boneOrder(std::move(boneOrder)),
                                                                                       correction(correction)
        {
        }

        uint32 GetJointCount() const { return jointCount; }
        const Joint &GetHeadJoint() const { return headJoint; }

    private:
        Joint LoadJointData(const Joint &jointNode, bool isRoot);
        Joint ExtractMainJointData(const Joint &jointNode, bool isRoot);
        std::optional<uint32> GetBoneIndex(const std::string &name) const;

        std::vector<std::string> boneOrder;
        Mat4 correction;

        uint32 jointCount = 0;
        Joint headJoint;
    };
}