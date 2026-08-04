#include "Transform.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace SF::Engine
{
    Transform::Transform(const Vector3float& position, const Vector3float& rotation,
                         const Vector3float& scale)
        : position(position), rotation(rotation), scale(scale)
    {
    }

    Matrix4float Transform::GetWorldMatrix() const
    {
        Transform world = GetWorldTransform();

        Matrix4float matrix(1.0f);
        matrix = glm::translate(matrix, world.position);
        matrix = glm::rotate(matrix, world.rotation.x, Vector3float(1, 0, 0));
        matrix = glm::rotate(matrix, world.rotation.y, Vector3float(0, 1, 0));
        matrix = glm::rotate(matrix, world.rotation.z, Vector3float(0, 0, 1));
        matrix = glm::scale(matrix, world.scale);
        return matrix;
    }

    Vector3float Transform::GetPosition() const { return GetWorldTransform().position; }
    Vector3float Transform::GetRotation() const { return GetWorldTransform().rotation; }
    Vector3float Transform::GetScale()    const { return GetWorldTransform().scale; }

    bool Transform::operator==(const Transform& rhs) const
    {
        return position == rhs.position && rotation == rhs.rotation && scale == rhs.scale;
    }

    bool Transform::operator!=(const Transform& rhs) const
    {
        return !operator==(rhs);
    }

    Transform operator*(const Transform& lhs, const Transform& rhs)
    {
        Matrix4float lhsMatrix = lhs.GetWorldMatrix();
        Vector3float transformedPos = Vector3float(lhsMatrix * Vector4float(rhs.position, 1.0f));

        return {transformedPos, lhs.rotation + rhs.rotation, lhs.scale * rhs.scale};
    }

    Transform& Transform::operator*=(const Transform& rhs)
    {
        return *this = *this * rhs;
    }

    Transform Transform::GetWorldTransform() const
    {
        Transform local(position, rotation, scale);

        if (Entity* owner = GetOwner())
        {
            if (Entity* parent = owner->GetParent())
            {
                if (Transform* parentTransform = parent->GetComponent<Transform>())
                {
                    return (*parentTransform) * local; // recurse up the chain
                }
            }
        }

        return local; // root, or parent has no Transform
    }
}