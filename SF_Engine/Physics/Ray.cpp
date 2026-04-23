#include "Ray.hpp"

namespace SF::Engine
{
    Ray::Ray(bool useMouse, const Vector2float& screenStart)
        : useMouse(useMouse), screenStart(screenStart)
    {
    }

    void Ray::Update(const Vector3float& currentPosition, const Vector2float& mousePosition,
                     const Matrix4float& viewMatrix, const Matrix4float& projectionMatrix)
    {
        origin = currentPosition;

        if (useMouse)
            UpdateNormalizedDeviceCoordinates(mousePosition.x, mousePosition.y);
        else
            normalizedCoords = screenStart;

        this->viewMatrix = viewMatrix;
        this->projectionMatrix = projectionMatrix;
        clipCoords = Vector4float(normalizedCoords.x, normalizedCoords.y, -1.0f, 1.0f);
        UpdateEyeCoords();
        UpdateWorldCoords();
    }

    Vector3float Ray::GetPointOnRay(float distance) const
    {
        auto vector = distance * currentRay;
        return origin + vector;
    }

    Vector3float Ray::ConvertToScreenSpace(const Vector3float& position) const
    {
        Vector4float coords(position, 1.0f);
        coords = viewMatrix * coords;
        coords = projectionMatrix * coords;

        if (coords.w < 0.0f) return Vector3float(0.0f);

        return Vector3float((coords.x / coords.w + 1.0f) / 2.0f,
                            1.0f - (coords.y / coords.w + 1.0f) / 2.0f, coords.z);
    }

    void Ray::UpdateNormalizedDeviceCoordinates(float mouseX, float mouseY)
    {
        normalizedCoords.x = (2.0f * mouseX) - 1.0f;
        normalizedCoords.y = (2.0f * mouseY) - 1.0f;
    }

    void Ray::UpdateEyeCoords()
    {
        invertedProjection = glm::inverse(projectionMatrix);
        eyeCoords = invertedProjection * clipCoords;
        eyeCoords = Vector4float(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);
    }

    void Ray::UpdateWorldCoords()
    {
        invertedView = glm::inverse(viewMatrix);
        rayWorld = invertedView * eyeCoords;
        currentRay = Vector3float(rayWorld);
    }
}