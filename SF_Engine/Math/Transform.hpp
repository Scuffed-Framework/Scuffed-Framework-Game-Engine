#pragma once

#include <Math/Vectors/Vector.hpp>
#include <Components/Component.hpp>
#include <Math/Matrix/Matrix4.hpp>
#include <XML/XMLModule.hpp>
#include <Scene/SceneSerialization.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace SF::Engine
{
    /**
     * @brief Holds position, rotation, and scale components.
     */
    class Transform : public Component::Registrar<Transform>
    {
        inline static const bool Registered = Register("Transform");

    public:
        /**
         * Creates a new transform.
         * @param position The position.
         * @param rotation The rotation.
         * @param scale The scale.
         */
        Transform(const Vec3 &position = {}, const Vec3 &rotation = {}, const Vec3 &scale = Vec3(1.0f));
        ~Transform() = default;

        std::string_view GetTypeName() const override {return "Transform";}

        Mat4 GetWorldMatrix() const;
        Vec3 GetPosition() const;
        Vec3 GetRotation() const;
        Vec3 GetScale() const;

        const Vec3 &GetLocalPosition() const { return position; }
        void SetLocalPosition(const Vec3 &localPosition) { position = localPosition; }

        const Vec3 &GetLocalRotation() const { return rotation; }
        void SetLocalRotation(const Vec3 &localRotation) { rotation = localRotation; }

        const Vec3 &GetLocalScale() const { return scale; }
        void SetLocalScale(const Vec3 &localScale) { scale = localScale; }

        bool operator==(const Transform &rhs) const;
        bool operator!=(const Transform &rhs) const;

        friend Transform operator*(const Transform &lhs, const Transform &rhs);

        Transform &operator*=(const Transform &rhs);

        Transform GetWorldTransform() const;

        Vec3 position;
        Vec3 rotation;
        Vec3 scale;

        mutable Transform *worldTransform = nullptr;
        void Serialize(XMLNode &node) const override
        {
            Component::Serialize(node);
            SerializeVec3(node, "position", position);
            SerializeVec3(node, "rotation", rotation);
            SerializeVec3(node, "scale", scale);
        }

        void Deserialize(const XMLNode &node) override
        {
            Component::Deserialize(node);
            position = DeserializeVec3(node, "position");
            rotation = DeserializeVec3(node, "rotation");
            scale = DeserializeVec3(node, "scale", {1.f, 1.f, 1.f});
        }

        Mat4 ToMatrix() const
        {
            Mat4 T = glm::translate(Mat4(1.0f), position);
            Mat4 R = glm::yawPitchRoll(
                glm::radians(rotation.y),
                glm::radians(rotation.x),
                glm::radians(rotation.z));
            Mat4 S = glm::scale(Mat4(1.0f), scale);
            return T * R * S;
        }

        void Reset() override
        {
            position = {0.0f, 0.0f, 0.0f};
            rotation = {0.0f, 0.0f, 0.0f};
            scale = {1.0f, 1.0f, 1.0f};
        }
    };
}