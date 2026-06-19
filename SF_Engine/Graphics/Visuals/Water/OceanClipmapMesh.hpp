#pragma once

#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Buffers/Buffer.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace SF::Engine
{
    /**
     * @brief Concentric LOD-ring ("clipmap") ocean mesh for true infinite
     *        coverage to the horizon, projected onto the planet sphere.
     *
     * Each ring is a square annulus of quad patches at a doubling world
     * scale (ring i covers [2^i * baseExtent, 2^(i+1) * baseExtent] roughly),
     * all sharing the same (patchCount+1)^2-ish vertex topology but scaled
     * and recentered under the camera every frame. Innermost ring is the
     * finest; FFT cascade 3 (capillary) only contributes near the camera,
     * cascade 0 (swell) contributes everywhere.
     *
     * Vertices are generated in a camera-relative tangent plane and
     * projected onto the sphere exactly as in OceanTessellatedMesh, so this
     * is a drop-in topology upgrade.
     */
    class OceanClipmapMesh
    {
    public:
        // ringCount: number of concentric LOD rings (4-6 typical).
        // baseExtent: world-space size of the innermost ring.
        // patchCount: subdivisions per axis, per ring.
        explicit OceanClipmapMesh(
            uint32_t ringCount = 5,
            float baseExtent = 200.0f,
            uint32_t patchCount = 32);

        ~OceanClipmapMesh() = default;

        OceanClipmapMesh(const OceanClipmapMesh &) = delete;
        OceanClipmapMesh &operator=(const OceanClipmapMesh &) = delete;

        void RegenerateAt(const glm::vec3 &cameraPos,
                          const glm::vec3 &planetCenter,
                          float planetRadius);

        void Draw(const CommandBuffer &cmd) const;

        uint32_t GetRingCount() const { return ringCount_; }

    private:
        struct Ring
        {
            std::unique_ptr<Buffer> vertexBuffer;
            std::unique_ptr<Buffer> indexBuffer;
            uint32_t vertexCount = 0;
            uint32_t indexCount = 0;
            float extent = 0.0f;    // outer extent of this ring
            float innerHole = 0.0f; // inner extent excluded (covered by next ring in)
        };

        void buildRingTopology(uint32_t ringIndex);
        void writeRingVertices(uint32_t ringIndex,
                               const glm::vec3 &origin,
                               const glm::vec3 &tangentU,
                               const glm::vec3 &tangentV,
                               const glm::vec3 &planetCenter,
                               float planetRadius);

        uint32_t ringCount_;
        float baseExtent_;
        uint32_t patchCount_;
        std::vector<Ring> rings_;
    };
}