#include "Transform.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <Entity/Entity.hpp>

namespace SF::Engine
{
    Transform::Transform(const Vec3& position, const Vec3& rotation,
                         const Vec3& scale)
        : position(position), rotation(rotation), scale(scale)
    {
    }

    Mat4 Transform::GetWorldMatrix() const
    {
        Transform world = GetWorldTransform();

        Mat4 matrix(1.0f);
        matrix = glm::translate(matrix, world.position);
        matrix = glm::rotate(matrix, world.rotation.x, Vec3(1, 0, 0));
        matrix = glm::rotate(matrix, world.rotation.y, Vec3(0, 1, 0));
        matrix = glm::rotate(matrix, world.rotation.z, Vec3(0, 0, 1));
        matrix = glm::scale(matrix, world.scale);
        return matrix;
    }

    Vec3 Transform::GetPosition() const { return GetWorldTransform().position; }
    Vec3 Transform::GetRotation() const { return GetWorldTransform().rotation; }
    Vec3 Transform::GetScale()    const { return GetWorldTransform().scale; }

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
        Mat4 lhsMatrix = lhs.GetWorldMatrix();
        Vec3 transformedPos = Vec3(lhsMatrix * Vec4(rhs.position, 1.0f));

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