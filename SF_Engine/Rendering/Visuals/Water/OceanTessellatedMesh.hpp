#pragma once

#include <Rendering/Mesh/Mesh.hpp>
#include <Rendering/Buffers/Buffer.hpp>
#include <Math/BasicMath.hpp>
#include <memory>

namespace SF::Engine
{
    /**
     * @brief Coarse quad-patch grid for GPU tessellation, projected onto a
     *        sphere and recentered under the camera every frame.
     *
     * The grid is built in a tangent plane at the point on the planet's
     * surface directly "below" the camera (i.e. along -normalize(cameraPos
     * - planetCenter)), then each vertex is projected outward onto the
     * sphere of radius `planetRadius`. This gives an "infinite ocean"
     * illusion: the grid always extends to the horizon around the camera,
     * curved to match the planet, with no fixed world-space extents.
     *
     * Gerstner wave displacement is still applied per-vertex in the
     * tessellation evaluation shader, on top of this curved base position.
     *
     * Vertex layout matches SF::Engine::PatchVertex (location 0 = position,
     * 1 = uv, 2 = normal) so PatchVertex::GetVertexInput() applies directly.
     *
     * Control-point ordering per patch (CCW, as seen from above the tangent
     * plane):
     *   [0] = bottom-left   (u=0, v=0)
     *   [1] = bottom-right  (u=1, v=0)
     *   [2] = top-right     (u=1, v=1)
     *   [3] = top-left      (u=0, v=1)
     */
    class OceanTessellatedMesh
    {
    public:
        /**
         * @param patchCount   Grid subdivisions per axis; total patches = patchCount².
         *                     E.g. 32 -> 1024 quad patches fed to the tessellator.
         * @param patchExtent  World-space size (X/Z in the tangent plane) of the
         *                     whole grid, in engine units. Should be sized so the
         *                     grid covers out to roughly the horizon distance for
         *                     typical camera altitudes.
         */
        explicit OceanTessellatedMesh(
            uint32_t patchCount = 32,
            const Vec2 &patchExtent = Vec2(4000.f, 4000.f));

        ~OceanTessellatedMesh() = default;

        OceanTessellatedMesh(const OceanTessellatedMesh &) = delete;
        OceanTessellatedMesh &operator=(const OceanTessellatedMesh &) = delete;
        OceanTessellatedMesh(OceanTessellatedMesh &&) = default;
        OceanTessellatedMesh &operator=(OceanTessellatedMesh &&) = default;

        /**
         * Rebuild the grid centered under cameraPos, projected onto the
         * sphere (planetCenter, planetRadius). Call once per frame before
         * Draw(). Cheap: (patchCount+1)^2 vertices, CPU-side rewrite +
         * buffer upload (host-visible, sequential write).
         *
         * @param cameraPos     Camera position in world space.
         * @param planetCenter  Planet center in world space.
         * @param planetRadius  Planet (sea-level) radius in world units.
         */
        void RegenerateAt(const Vec3 &cameraPos,
                          const Vec3 &planetCenter,
                          float planetRadius);

        /**
         * Bind vertex / index buffers and issue vkCmdDrawIndexed.
         * Must be called inside an active render pass bound with a
         * PATCH_LIST pipeline (patchControlPoints = 4).
         */
        void Draw(const CommandBuffer &cmd, uint32_t instanceCount = 1) const;

        uint32_t GetTotalPatches() const { return patchCount_ * patchCount_; }
        uint32_t GetVertexCount() const { return vertexCount_; }
        uint32_t GetIndexCount() const { return indexCount_; }

        const std::unique_ptr<Buffer> &GetVertexBuffer() const { return vertexBuffer_; }
        const std::unique_ptr<Buffer> &GetIndexBuffer() const { return indexBuffer_; }

    private:
        // Builds index buffer once (topology never changes) and does an
        // initial vertex generation centered at the origin.
        void generateInitial();

        // Rebuilds only the vertex buffer contents for the given tangent
        // frame / sphere projection. Shared by the constructor and
        // RegenerateAt().
        void writeVertices(const Vec3 &origin,
                           const Vec3 &tangentU,
                           const Vec3 &tangentV,
                           const Vec3 &planetCenter,
                           float planetRadius);

        uint32_t patchCount_;
        Vec2 patchExtent_;

        std::unique_ptr<Buffer> vertexBuffer_;
        std::unique_ptr<Buffer> indexBuffer_;
        uint32_t vertexCount_ = 0;
        uint32_t indexCount_ = 0;
    };

} // namespace SF::Engine
