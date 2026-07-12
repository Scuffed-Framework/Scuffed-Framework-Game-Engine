#pragma once
#include <Math/Matrix/Matrix4.hpp>
#include <Math/Vectors/Vector3.hpp>
#include <Math/Quaternion/Quaternion.hpp>
#include <Reflection/RTTISingle.hpp>

#include <TemplateLibrary/Types.hpp>
#include <TemplateLibrary/TypeTraits.hpp>

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
            Joint(uint32_t index = 0, std::string name = "", const Matrix4float &bindLocalTransform = Matrix4float(0)) : index(index),
                                                                                                                         name(::SFTL::move(name)),
                                                                                                                         localBindTransform(bindLocalTransform)
            {
            }

            void CalculateInverseBindTransform(const Matrix4float &parentBindTransform)
            {
                auto bindTransform = parentBindTransform * localBindTransform;
                inverseBindTransform = ::glm::inverse(bindTransform);

                for (auto &child : children)
                    child.CalculateInverseBindTransform(bindTransform);
            }

            uint32_t GetIndex() const { return index; }
            void SetIndex(uint32_t index) { this->index = index; }

            const std::string &GetName() const { return name; }
            void SetName(const std::string &name) { this->name = name; }

            const std::vector<Joint> &GetChildren() const { return children; }

            void AddChild(const Joint &child) { children.emplace_back(child); }

            const Matrix4float &GetLocalBindTransform() const { return localBindTransform; }
            void SetLocalBindTransform(const Matrix4float &localBindTransform) { this->localBindTransform = localBindTransform; }
            const Matrix4float &GetInverseBindTransform() const { return inverseBindTransform; }
            void SetInverseBindTransform(const Matrix4float &inverseBindTransform) { this->inverseBindTransform = inverseBindTransform; }

        private:
            uint32_t index = 0;
            std::string name;
            std::vector<Joint> children;

            Matrix4float localBindTransform;
            Matrix4float inverseBindTransform;
        };

#pragma once

        class JointTransform
        {
        public:
            JointTransform() = default;

            JointTransform(const Vector3float &position, const Quaternion &rotation);

            explicit JointTransform(const Matrix4float &localTransform);

            Matrix4float GetLocalTransform() const;

            static JointTransform Interpolate(const JointTransform &frameA, const JointTransform &frameB, float progression);

            static Vector3float Interpolate(const Vector3float &start, const Vector3float &end, float progression);

            const Vector3float &GetPosition() const { return position; }
            void SetPosition(const Vector3float &position) { this->position = position; }

            const Quaternion &GetRotation() const { return rotation; }
            void SetRotation(const Quaternion &rotation) { this->rotation = rotation; }

        private:
            Vector3float position;
            Quaternion rotation;
        };
    }
}