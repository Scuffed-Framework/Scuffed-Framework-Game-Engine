#include "OceanTessellatedMesh.hpp"

#include <Rendering/Mesh/Vertex.hpp> // SF::Engine::PatchVertex
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace SF::Engine
{
    OceanTessellatedMesh::OceanTessellatedMesh(uint32_t patchCount, const Vec2 &patchExtent) :
        patchCount_(patchCount), patchExtent_(patchExtent)
    {
        if (patchCount < 1)
            throw std::runtime_error("OceanTessellatedMesh: patchCount must be >= 1");

        generateInitial();
    }

    void OceanTessellatedMesh::generateInitial()
    {
        const uint32_t gridW = patchCount_ + 1; // vertices per axis
        vertexCount_         = gridW * gridW;
        indexCount_          = patchCount_ * patchCount_ * 4;

        std::vector<uint32_t> indices;
        indices.reserve(indexCount_);

        for (uint32_t row = 0; row < patchCount_; ++row)
        {
            for (uint32_t col = 0; col < patchCount_; ++col)
            {
                const uint32_t bl = row * gridW + col;           // [0] BL
                const uint32_t br = row * gridW + col + 1;       // [1] BR
                const uint32_t tr = (row + 1) * gridW + col + 1; // [2] TR
                const uint32_t tl = (row + 1) * gridW + col;     // [3] TL

                indices.push_back(bl);
                indices.push_back(br);
                indices.push_back(tr);
                indices.push_back(tl);
            }
        }

        indexBuffer_ = std::make_unique<Buffer>(
                sizeof(uint32_t) * indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, std::as_bytes(std::span(indices)));

        std::vector<PatchVertex> vertices(vertexCount_);

        const float halfX = patchExtent_.x * 0.5f;
        const float halfZ = patchExtent_.y * 0.5f;
        const float stepX = patchExtent_.x / static_cast<float>(patchCount_);
        const float stepZ = patchExtent_.y / static_cast<float>(patchCount_);
        const float invN  = 1.0f / static_cast<float>(patchCount_);

        for (uint32_t row = 0; row < gridW; ++row)
        {
            for (uint32_t col = 0; col < gridW; ++col)
            {
                PatchVertex &v = vertices[row * gridW + col];
                v.position =
                        Vec3(-halfX + static_cast<float>(col) * stepX, 0.0f, -halfZ + static_cast<float>(row) * stepZ);
                v.uv     = Vec2(static_cast<float>(col) * invN, static_cast<float>(row) * invN);
                v.normal = Vec3(0.0f, 1.0f, 0.0f);
            }
        }

        vertexBuffer_ = std::make_unique<Buffer>(
                sizeof(PatchVertex) * vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, std::as_bytes(std::span(vertices)));
    }

    void OceanTessellatedMesh::writeVertices(const Vec3 &origin, const Vec3 &tangentU, const Vec3 &tangentV,
                                             const Vec3 &planetCenter, float planetRadius) const
    {
        const uint32_t gridW = patchCount_ + 1;

        std::vector<PatchVertex> vertices(vertexCount_);

        const float halfX = patchExtent_.x * 0.5f;
        const float halfZ = patchExtent_.y * 0.5f;
        const float stepX = patchExtent_.x / static_cast<float>(patchCount_);
        const float stepZ = patchExtent_.y / static_cast<float>(patchCount_);
        const float invN  = 1.0f / static_cast<float>(patchCount_);

        for (uint32_t row = 0; row < gridW; ++row)
        {
            for (uint32_t col = 0; col < gridW; ++col)
            {
                const float localX = -halfX + static_cast<float>(col) * stepX;
                const float localZ = -halfZ + static_cast<float>(row) * stepZ;

                // Point on the flat tangent plane.
                const Vec3 flatPos = origin + tangentU * localX + tangentV * localZ;

                // Project outward onto the sphere: direction from planet
                // center through the flat-plane point, scaled to radius.
                const Vec3 dir       = normalize(flatPos - planetCenter);
                const Vec3 spherePos = planetCenter + dir * planetRadius;

                PatchVertex &v = vertices[row * gridW + col];
                v.position     = spherePos;
                v.uv           = Vec2(static_cast<float>(col) * invN, static_cast<float>(row) * invN);
                // Outward radial normal; Gerstner waves perturb this further
                // in the tessellation eval / fragment stages.
                v.normal = dir;
            }
        }

        // Rewrite the host-visible vertex buffer in place.
        void *mapped = nullptr;
        vertexBuffer_->MapMemory(&mapped);
        std::memcpy(mapped, vertices.data(), sizeof(PatchVertex) * vertices.size());
        vertexBuffer_->FlushMemory();
        vertexBuffer_->UnmapMemory();
    }

    void OceanTessellatedMesh::RegenerateAt(const Vec3 &cameraPos, const Vec3 &planetCenter, float planetRadius) const
    {
        // Direction from planet center to camera ("up" at the camera's location).
        Vec3 up           = cameraPos - planetCenter;
        const float upLen = glm::length(up);
        if (upLen < 1e-5f)
        {
            // Degenerate (camera at planet center), fall back to world up.
            up = Vec3(0.0f, 1.0f, 0.0f);
        } else
        {
            up /= upLen;
        }

        // Point on the sphere directly "below" the camera.
        const Vec3 surfacePoint = planetCenter + up * planetRadius;

        // Build an arbitrary orthonormal tangent basis at surfacePoint.
        Vec3 reference = Vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(reference, up)) > 0.99f)
            reference = Vec3(1.0f, 0.0f, 0.0f);

        const Vec3 tangentU = normalize(cross(reference, up));
        const Vec3 tangentV = normalize(cross(up, tangentU));

        writeVertices(surfacePoint, tangentU, tangentV, planetCenter, planetRadius);
    }

    void OceanTessellatedMesh::Draw(const CommandBuffer &cmd, uint32_t instanceCount) const
    {
        VkBuffer vb      = vertexBuffer_->GetBuffer();
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cmd, indexBuffer_->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, indexCount_, instanceCount, 0, 0, 0);
    }

} // namespace SF::Engine
