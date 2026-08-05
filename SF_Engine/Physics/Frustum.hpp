#pragma once

#include <array>

#include <Math/Matrix/Matrix4.hpp>
#include <Math/Vectors/Vector.hpp>

namespace SF::Engine
{
    /**
     * @brief Represents the region of space in the modeled world that may appear on the screen.
     */
    class Frustum
    {
    public:
        Frustum() = default;

        /**
         * Updates a frustum from the view and projection matrix.
         * @param view The view matrix.
         * @param projection The projection matrix.
         */
        void Update(const Mat4& view, const Mat4& projection);

        /**
         * Gets if a point contained in the frustum.
         * @param position The point.
         * @return If the point is contained.
         */
        bool PointInFrustum(const Vec3& position) const;

        /**
         * Gets if a sphere contained in the frustum.
         * @param position The spheres position.
         * @param radius The spheres radius.
         * @return If the sphere is contained.
         */
        bool SphereInFrustum(const Vec3& position, float radius) const;

        /**
         * Gets if a cube contained in the frustum.
         * @param min The cube min point.
         * @param max The cube max point.
         * @return If cube sphere is contained.
         */
        bool CubeInFrustum(const Vec3& min, const Vec3& max) const;

    private:
        void NormalizePlane(int32_t side);

        std::array<std::array<float, 4>, 6> frustum = {};
    };
}