#pragma once

#include <Math/Matrix/Matrix4.hpp>
#include <Math/Vectors/Vector.hpp>

namespace SF::Engine
{
    /**
     * @brief Class that represents a 3 dimensional ray.
     */
    class Ray
    {
    public:
        /**
         * Creates a new ray.
         * @param useMouse If the ray will use the mouse coords or to start from screenStart.
         * @param screenStart If useMouse is false then this will be used as the rays start.
         */
        Ray(bool useMouse, const Vector2float& screenStart);

        /**
         * Updates the ray to a new position.
         * @param currentPosition The origin of the ray.
         * @param mousePosition The mouses screen space position.
         * @param viewMatrix The cameras view matrix.
         * @param projectionMatrix The projection view matrix.
         */
        void Update(const Vector3float& currentPosition, const Vector2float& mousePosition,
                    const Matrix4float& viewMatrix, const Matrix4float& projectionMatrix);

        /**
         * Gets a point on the ray.
         * @param distance Distance down the ray to sample.
         * @return The destination vector.
         */
        Vector3float GetPointOnRay(float distance) const;

        /**
         * Converts a position from world space to screen space.
         * @param position The position to convert.
         * @return The destination vector X and Y being screen space coords and Z being the distance
         * to the camera.
         */
        Vector3float ConvertToScreenSpace(const Vector3float& position) const;

        bool IsUseMouse() const
        {
            return useMouse;
        }
        void SetUseMouse(bool useMouse)
        {
            this->useMouse = useMouse;
        }

        const Vector2float& GetScreenStart() const
        {
            return screenStart;
        }
        void SetScreenStart(const Vector2float& screenStart)
        {
            this->screenStart = screenStart;
        }

        const Vector3float& GetOrigin() const
        {
            return origin;
        }
        const Vector3float& GetCurrentRay() const
        {
            return currentRay;
        }

    private:
        void UpdateNormalizedDeviceCoordinates(float mouseX, float mouseY);
        void UpdateEyeCoords();
        void UpdateWorldCoords();

        bool useMouse;
        Vector2float screenStart;

        Matrix4float projectionMatrix, viewMatrix;

        Vector2float normalizedCoords;
        Vector4float clipCoords;
        Vector4float eyeCoords;

        Matrix4float invertedProjection, invertedView;
        Vector4float rayWorld;

        Vector3float origin;
        Vector3float currentRay;
    };
}