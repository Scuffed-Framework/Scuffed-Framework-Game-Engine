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
        Ray(bool useMouse, const Vec2& screenStart);

        /**
         * Updates the ray to a new position.
         * @param currentPosition The origin of the ray.
         * @param mousePosition The mouses screen space position.
         * @param viewMatrix The cameras view matrix.
         * @param projectionMatrix The projection view matrix.
         */
        void Update(const Vec3& currentPosition, const Vec2& mousePosition,
                    const Mat4& viewMatrix, const Mat4& projectionMatrix);

        /**
         * Gets a point on the ray.
         * @param distance Distance down the ray to sample.
         * @return The destination vector.
         */
        Vec3 GetPointOnRay(float distance) const;

        /**
         * Converts a position from world space to screen space.
         * @param position The position to convert.
         * @return The destination vector X and Y being screen space coords and Z being the distance
         * to the camera.
         */
        Vec3 ConvertToScreenSpace(const Vec3& position) const;

        bool IsUseMouse() const
        {
            return useMouse;
        }
        void SetUseMouse(bool useMouse)
        {
            this->useMouse = useMouse;
        }

        const Vec2& GetScreenStart() const
        {
            return screenStart;
        }
        void SetScreenStart(const Vec2& screenStart)
        {
            this->screenStart = screenStart;
        }

        const Vec3& GetOrigin() const
        {
            return origin;
        }
        const Vec3& GetCurrentRay() const
        {
            return currentRay;
        }

    private:
        void UpdateNormalizedDeviceCoordinates(float mouseX, float mouseY);
        void UpdateEyeCoords();
        void UpdateWorldCoords();

        bool useMouse;
        Vec2 screenStart;

        Mat4 projectionMatrix, viewMatrix;

        Vec2 normalizedCoords;
        Vec4 clipCoords;
        Vec4 eyeCoords;

        Mat4 invertedProjection, invertedView;
        Vec4 rayWorld;

        Vec3 origin;
        Vec3 currentRay;
    };
}