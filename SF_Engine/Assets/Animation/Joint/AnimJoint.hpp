#pragma once
#include <LowLevel/Reflection/RTTISingle.hpp>
#include <Math/Matrix/Matrix4.hpp>
#include <Math/Quaternion/Quaternion.hpp>
#include <Math/Vectors/Vector3.hpp>

#include <1stPartyLibs/TemplateLibrary/DynamicArray.hpp>
#include <1stPartyLibs/TemplateLibrary/TypeTraits.hpp>
#include <1stPartyLibs/TemplateLibrary/Types.hpp>

#include <1stPartyLibs/TemplateLibrary/Containers/String.hpp>

using namespace SFTL;
namespace SF::Engine
{
    namespace Animation
    {
        class Joint
        {
            SF_RTTI_BASE(Joint)
        public:
            // give no data cuz
            explicit Joint(uint32 index = 0, ::SFTL::string name = "", const Mat4 &bindLocalTransform = Mat4(0)) :
                index(index), name(::SFTL::move(name)), localBindTransform(bindLocalTransform)
            {
            }

            void CalculateInverseBindTransform(const Mat4 &parentBindTransform)
            {
                auto bindTransform   = parentBindTransform * localBindTransform;
                inverseBindTransform = inverse(bindTransform);

                for (auto &child: children)
                    child.CalculateInverseBindTransform(bindTransform);
            }

            [[nodiscard]] uint32 GetIndex() const { return index; }
            void SetIndex(uint32 index) { this->index = index; }

            [[nodiscard]] const ::SFTL::string &GetName() const { return name; }
            void SetName(const ::SFTL::string &name) { this->name = name; }

            [[nodiscard]] const DynamicArray<Joint> &GetChildren() const { return children; }

            void AddChild(const Joint &child) { children.emplace_back(child); }

            [[nodiscard]] const Mat4 &GetLocalBindTransform() const { return localBindTransform; }
            void SetLocalBindTransform(const Mat4 &localBindTransform)
            {
                this->localBindTransform = localBindTransform;
            }
            [[nodiscard]] const Mat4 &GetInverseBindTransform() const { return inverseBindTransform; }
            void SetInverseBindTransform(const Mat4 &inverseBindTransform)
            {
                this->inverseBindTransform = inverseBindTransform;
            }

        private:
            uint32_t index = 0;
            ::SFTL::string name;
            DynamicArray<Joint> children;

            Mat4 localBindTransform;
            Mat4 inverseBindTransform;
        };

        class JointTransform
        {
        public:
            JointTransform() = default;

            JointTransform(const Vec3 &position, const Quaternion &rotation);

            explicit JointTransform(const Mat4 &localTransform);

            [[nodiscard]] Mat4 GetLocalTransform() const;

            static JointTransform Interpolate(const JointTransform &frameA, const JointTransform &frameB,
                                              float progression);

            static Vec3 Interpolate(const Vec3 &start, const Vec3 &end, float progression);

            [[nodiscard]] const Vec3 &GetPosition() const { return position; }
            void SetPosition(const Vec3 &position) { this->position = position; }

            [[nodiscard]] const Quaternion &GetRotation() const { return rotation; }
            void SetRotation(const Quaternion &rotation) { this->rotation = rotation; }

        private:
            Vec3 position;
            Quaternion rotation;
        };
    } // namespace Animation
} // namespace SF::Engine
