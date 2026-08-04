#pragma once

#include <Math/Vectors/Vector.hpp>
#include <Components/Component.hpp>
#include <Math/Matrix/Matrix4.hpp>
#include <XML/XMLReader.hpp>
#include <Scene/SceneSerialization.hpp>
#include <glm/gtx/euler_angles.hpp>

namespace SF::Engine
{
    /**
     * @brief Holds position, rotation, and scale components.
     */
    class Transform : public Component::Registrar<Transform>
    {
        inline static const bool Registered = Register("transform");

    public:
        /**
         * Creates a new transform.
         * @param position The position.
         * @param rotation The rotation.
         * @param scale The scale.
         */
        Transform(const Vector3float &position = {}, const Vector3float &rotation = {}, const Vector3float &scale = Vector3float(1.0f));
        ~Transform() = default;

        Matrix4float GetWorldMatrix() const;
        Vector3float GetPosition() const;
        Vector3float GetRotation() const;
        Vector3float GetScale() const;

        const Vector3float &GetLocalPosition() const { return position; }
        void SetLocalPosition(const Vector3float &localPosition) { position = localPosition; }

        const Vector3float &GetLocalRotation() const { return rotation; }
        void SetLocalRotation(const Vector3float &localRotation) { rotation = localRotation; }

        const Vector3float &GetLocalScale() const { return scale; }
        void SetLocalScale(const Vector3float &localScale) { scale = localScale; }

        bool operator==(const Transform &rhs) const;
        bool operator!=(const Transform &rhs) const;

        friend Transform operator*(const Transform &lhs, const Transform &rhs);

        Transform &operator*=(const Transform &rhs);

        Transform GetWorldTransform() const;

        Vector3float position;
        Vector3float rotation;
        Vector3float scale;

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

        Matrix4float ToMatrix() const
        {
            Matrix4float T = glm::translate(Matrix4float(1.0f), position);
            Matrix4float R = glm::yawPitchRoll(
                glm::radians(rotation.y),
                glm::radians(rotation.x),
                glm::radians(rotation.z));
            Matrix4float S = glm::scale(Matrix4float(1.0f), scale);
            return T * R * S;
        }

        void Reset()
        {
            position = {0.0f, 0.0f, 0.0f};
            rotation = {0.0f, 0.0f, 0.0f};
            scale = {1.0f, 1.0f, 1.0f};
        }
    };
}