#include "Transform.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace SF::Engine
{
    Transform::Transform(const Vector3float& position, const Vector3float& rotation,
                         const Vector3float& scale)
        : position(position), rotation(rotation), scale(scale)
    {
    }

    Transform::~Transform()
    {
        delete worldTransform;

        for (auto& child : children) child->parent = nullptr;

        if (parent) parent->RemoveChild(this);
    }

    Matrix4float Transform::GetWorldMatrix() const
    {
        auto worldTransform = GetWorldTransform();

        Matrix4float matrix = Matrix4float(1.0f);
        matrix = glm::translate(matrix, worldTransform->position);
        matrix = glm::rotate(matrix, worldTransform->rotation.x, Vector3float(1, 0, 0));
        matrix = glm::rotate(matrix, worldTransform->rotation.y, Vector3float(0, 1, 0));
        matrix = glm::rotate(matrix, worldTransform->rotation.z, Vector3float(0, 0, 1));
        matrix = glm::scale(matrix, worldTransform->scale);

        return matrix;
    }

    Vector3float Transform::GetPosition() const
    {
        return GetWorldTransform()->position;
    }

    Vector3float Transform::GetRotation() const
    {
        return GetWorldTransform()->rotation;
    }

    Vector3float Transform::GetScale() const
    {
        return GetWorldTransform()->scale;
    }

    void Transform::SetParent(Transform* parent)
    {
        if (this->parent) this->parent->RemoveChild(this);  // FIX: Remove from OLD parent

        this->parent = parent;

        if (parent) parent->AddChild(this);
    }

    // REMOVE THIS METHOD - it won't work with entt::entity
    // void Transform::SetParent(Entity* parent)
    // {
    //     SetParent(parent->GetComponent<Transform>());
    // }

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

    const Transform* Transform::GetWorldTransform() const
    {
        if (!parent)
        {
            if (worldTransform)
            {
                delete worldTransform;
                worldTransform = nullptr;
            }

            return this;
        }

        if (!worldTransform)
        {
            worldTransform = new Transform();
        }

        *worldTransform = *parent->GetWorldTransform() * *this;
        return worldTransform;
    }

    void Transform::AddChild(Transform* child)
    {
        children.emplace_back(child);
    }

    void Transform::RemoveChild(Transform* child)
    {
        children.erase(std::remove(children.begin(), children.end(), child), children.end());
    }
}