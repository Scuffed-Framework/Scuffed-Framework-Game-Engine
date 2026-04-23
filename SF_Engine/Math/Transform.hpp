#pragma once

#include <Math/Vectors/Vector.hpp>
#include <Scene/Component.hpp>
#include <Math/Matrix/Matrix4.hpp>

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
        ~Transform();

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

        Transform *GetParent() const { return parent; }
        void SetParent(Transform *parent);
        void SetParent(Entity *parent);

        const std::vector<Transform *> &GetChildren() const { return children; }

        bool operator==(const Transform &rhs) const;
        bool operator!=(const Transform &rhs) const;

        friend Transform operator*(const Transform &lhs, const Transform &rhs);

        Transform &operator*=(const Transform &rhs);

    private:
        const Transform *GetWorldTransform() const;

        void AddChild(Transform *child);
        void RemoveChild(Transform *child);

        Vector3float position;
        Vector3float rotation;
        Vector3float scale;

        Transform *parent = nullptr;
        std::vector<Transform *> children;
        mutable Transform *worldTransform = nullptr;
    };
}