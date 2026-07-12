#include "OceanClipmapMesh.hpp"
#include <Graphics/Mesh/Vertex.hpp> // SF::Engine::PatchVertex
#include <cmath>
#include <cstring>

namespace SF::Engine
{
    OceanClipmapMesh::OceanClipmapMesh(uint32_t ringCount, float baseExtent, uint32_t patchCount)
        : ringCount_(ringCount), baseExtent_(baseExtent), patchCount_(patchCount)
    {
        rings_.resize(ringCount_);

        for (uint32_t i = 0; i < ringCount_; ++i)
        {
            // Ring 0: solid square [0, baseExtent].
            // Ring i>0: square annulus, outer = baseExtent * 2^(i+1),
            //           inner hole = baseExtent * 2^i (covered by ring i-1).
            rings_[i].extent = baseExtent_ * static_cast<float>(1u << (i + 1));
            rings_[i].innerHole = (i == 0) ? 0.0f : baseExtent_ * static_cast<float>(1u << i);

            buildRingTopology(i);
        }
    }

    // Builds index buffer for ring i. Ring 0 is a full (patchCount+1)^2 grid.
    // Ring i>0 is the same grid but with the center hole's quads omitted
    // (that area is covered by the finer ring inside it), forming an
    // annulus of patches.
    void OceanClipmapMesh::buildRingTopology(uint32_t ringIndex)
    {
        Ring &ring = rings_[ringIndex];
        const uint32_t gridW = patchCount_ + 1;
        const uint32_t n = patchCount_;

        // Hole spans the central holeFrac fraction of the grid (in patch
        // units), centered. holeFrac = innerHole / extent.
        const float holeFrac = (ring.extent > 0.0f) ? (ring.innerHole / ring.extent) : 0.0f;
        const uint32_t holePatches = static_cast<uint32_t>(std::round(holeFrac * static_cast<float>(n)));
        const uint32_t holeStart = (n - holePatches) / 2;
        const uint32_t holeEnd = holeStart + holePatches; // [holeStart, holeEnd) excluded

        std::vector<uint32_t> indices;
        indices.reserve(n * n * 4);

        for (uint32_t row = 0; row < n; ++row)
        {
            for (uint32_t col = 0; col < n; ++col)
            {
                if (ringIndex > 0 &&
                    row >= holeStart && row < holeEnd &&
                    col >= holeStart && col < holeEnd)
                {
                    continue; // skip patches inside the hole, covered by inner ring
                }

                const uint32_t bl = row * gridW + col;
                const uint32_t br = row * gridW + col + 1;
                const uint32_t tr = (row + 1) * gridW + col + 1;
                const uint32_t tl = (row + 1) * gridW + col;

                indices.push_back(bl);
                indices.push_back(br);
                indices.push_back(tr);
                indices.push_back(tl);
            }
        }

        ring.indexCount = static_cast<uint32_t>(indices.size());
        ring.vertexCount = gridW * gridW;

        ring.indexBuffer = std::make_unique<Buffer>(
            sizeof(uint32_t) * indices.size(),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            std::as_bytes(std::span(indices)));

        // Allocate vertex buffer (filled by RegenerateAt before first draw).
        std::vector<PatchVertex> vertices(ring.vertexCount);
        ring.vertexBuffer = std::make_unique<Buffer>(
            sizeof(PatchVertex) * vertices.size(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            std::as_bytes(std::span(vertices)));
    }

    void OceanClipmapMesh::writeRingVertices(uint32_t ringIndex,
                                             const glm::vec3 &origin,
                                             const glm::vec3 &tangentU,
                                             const glm::vec3 &tangentV,
                                             const glm::vec3 &planetCenter,
                                             float planetRadius)
    {
        Ring &ring = rings_[ringIndex];
        const uint32_t gridW = patchCount_ + 1;
        const uint32_t n = patchCount_;

        const float extent = ring.extent;
        const float half = extent * 0.5f;
        const float step = extent / static_cast<float>(n);
        const float invN = 1.0f / static_cast<float>(n);

        std::vector<PatchVertex> vertices(ring.vertexCount);

        for (uint32_t row = 0; row < gridW; ++row)
        {
            for (uint32_t col = 0; col < gridW; ++col)
            {
                const float localX = -half + static_cast<float>(col) * step;
                const float localZ = -half + static_cast<float>(row) * step;

                const glm::vec3 flatPos = origin + tangentU * localX + tangentV * localZ;
                const glm::vec3 dir = glm::normalize(flatPos - planetCenter);
                const glm::vec3 spherePos = planetCenter + dir * planetRadius;

                PatchVertex &v = vertices[row * gridW + col];
                v.position = spherePos;
                v.uv = glm::vec2(static_cast<float>(col) * invN, static_cast<float>(row) * invN);
                v.normal = dir;
            }
        }

        void *mapped = nullptr;
        ring.vertexBuffer->MapMemory(&mapped);
        std::memcpy(mapped, vertices.data(), sizeof(PatchVertex) * vertices.size());
        ring.vertexBuffer->FlushMemory();
        ring.vertexBuffer->UnmapMemory();
    }

    void OceanClipmapMesh::RegenerateAt(const glm::vec3 &cameraPos,
                                        const glm::vec3 &planetCenter,
                                        float planetRadius)
    {
        glm::vec3 up = cameraPos - planetCenter;
        const float upLen = glm::length(up);
        up = (upLen < 1e-5f) ? glm::vec3(0.0f, 1.0f, 0.0f) : (up / upLen);

        const glm::vec3 surfacePoint = planetCenter + up * planetRadius;

        glm::vec3 reference = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(reference, up)) > 0.99f)
            reference = glm::vec3(1.0f, 0.0f, 0.0f);

        const glm::vec3 tangentU = glm::normalize(glm::cross(reference, up));
        const glm::vec3 tangentV = glm::normalize(glm::cross(up, tangentU));

        for (uint32_t i = 0; i < ringCount_; ++i)
            writeRingVertices(i, surfacePoint, tangentU, tangentV, planetCenter, planetRadius);
    }

    void OceanClipmapMesh::Draw(const CommandBuffer &cmd) const
    {
        for (const Ring &ring : rings_)
        {
            VkBuffer vb = ring.vertexBuffer->GetBuffer();
            VkDeviceSize off = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
            vkCmdBindIndexBuffer(cmd, ring.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, ring.indexCount, 1, 0, 0, 0);
        }
    }
}