#include "OceanTessellatedMesh.hpp"

#define SF_OCEAN_DEBUG_LOG

#include <Graphics/Mesh/Vertex.hpp> // SF::Engine::PatchVertex
#include <stdexcept>
#include <cmath>
#include <cstring>
#include <cstdio>

namespace SF::Engine
{
    OceanTessellatedMesh::OceanTessellatedMesh(uint32_t patchCount, const glm::vec2 &patchExtent)
        : patchCount_(patchCount), patchExtent_(patchExtent)
    {
        if (patchCount < 1)
            throw std::runtime_error("OceanTessellatedMesh: patchCount must be >= 1");

        generateInitial();
    }

    void OceanTessellatedMesh::generateInitial()
    {
        const uint32_t gridW = patchCount_ + 1; // vertices per axis
        vertexCount_ = gridW * gridW;
        indexCount_ = patchCount_ * patchCount_ * 4;

        // ---- Index buffer (static; topology never changes) ----
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
            sizeof(uint32_t) * indices.size(),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            std::as_bytes(std::span(indices)));

        // ---- Initial vertex buffer: flat grid on XZ plane at the origin,
        // with an "up" normal. Real positions get overwritten by
        // RegenerateAt() before the first frame is rendered, but we need a
        // valid, correctly-sized buffer to allocate against. ----
        std::vector<PatchVertex> vertices(vertexCount_);

        const float halfX = patchExtent_.x * 0.5f;
        const float halfZ = patchExtent_.y * 0.5f;
        const float stepX = patchExtent_.x / static_cast<float>(patchCount_);
        const float stepZ = patchExtent_.y / static_cast<float>(patchCount_);
        const float invN = 1.0f / static_cast<float>(patchCount_);

        for (uint32_t row = 0; row < gridW; ++row)
        {
            for (uint32_t col = 0; col < gridW; ++col)
            {
                PatchVertex &v = vertices[row * gridW + col];
                v.position = glm::vec3(
                    -halfX + static_cast<float>(col) * stepX,
                    0.0f,
                    -halfZ + static_cast<float>(row) * stepZ);
                v.uv = glm::vec2(
                    static_cast<float>(col) * invN,
                    static_cast<float>(row) * invN);
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }

        vertexBuffer_ = std::make_unique<Buffer>(
            sizeof(PatchVertex) * vertices.size(),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            std::as_bytes(std::span(vertices)));
    }

    void OceanTessellatedMesh::writeVertices(const glm::vec3 &origin,
                                             const glm::vec3 &tangentU,
                                             const glm::vec3 &tangentV,
                                             const glm::vec3 &planetCenter,
                                             float planetRadius)
    {
        const uint32_t gridW = patchCount_ + 1;

        std::vector<PatchVertex> vertices(vertexCount_);

        const float halfX = patchExtent_.x * 0.5f;
        const float halfZ = patchExtent_.y * 0.5f;
        const float stepX = patchExtent_.x / static_cast<float>(patchCount_);
        const float stepZ = patchExtent_.y / static_cast<float>(patchCount_);
        const float invN = 1.0f / static_cast<float>(patchCount_);

        for (uint32_t row = 0; row < gridW; ++row)
        {
            for (uint32_t col = 0; col < gridW; ++col)
            {
                const float localX = -halfX + static_cast<float>(col) * stepX;
                const float localZ = -halfZ + static_cast<float>(row) * stepZ;

                // Point on the flat tangent plane.
                const glm::vec3 flatPos = origin + tangentU * localX + tangentV * localZ;

                // Project outward onto the sphere: direction from planet
                // center through the flat-plane point, scaled to radius.
                const glm::vec3 dir = glm::normalize(flatPos - planetCenter);
                const glm::vec3 spherePos = planetCenter + dir * planetRadius;

                PatchVertex &v = vertices[row * gridW + col];
                v.position = spherePos;
                v.uv = glm::vec2(
                    static_cast<float>(col) * invN,
                    static_cast<float>(row) * invN);
                // Outward radial normal; Gerstner waves perturb this further
                // in the tessellation eval / fragment stages.
                v.normal = dir;
            }
        }

#ifdef SF_OCEAN_DEBUG_LOG
        {
            static int s_logCount = 0;
            if (s_logCount < 5)
            {
                ++s_logCount;
                const PatchVertex &v0 = vertices[0];
                const PatchVertex &vLast = vertices[vertices.size() - 1];
                printf("[OceanDebug] vertex[0].position=(%.3f,%.3f,%.3f) len=%.3f\n",
                       v0.position.x, v0.position.y, v0.position.z, glm::length(v0.position));
                printf("[OceanDebug] vertex[last].position=(%.3f,%.3f,%.3f) len=%.3f\n",
                       vLast.position.x, vLast.position.y, vLast.position.z, glm::length(vLast.position));
                printf("[OceanDebug] vertexCount=%u sizeof(PatchVertex)=%zu bufferSize=%zu vbSize=%llu\n",
                       (unsigned)vertices.size(), sizeof(PatchVertex),
                       sizeof(PatchVertex) * vertices.size(),
                       (unsigned long long)vertexBuffer_->GetSize());
            }
        }
#endif

        // Rewrite the host-visible vertex buffer in place.
        void *mapped = nullptr;
        vertexBuffer_->MapMemory(&mapped);
        std::memcpy(mapped, vertices.data(), sizeof(PatchVertex) * vertices.size());
        vertexBuffer_->FlushMemory();
        vertexBuffer_->UnmapMemory();
    }

    void OceanTessellatedMesh::RegenerateAt(const glm::vec3 &cameraPos,
                                            const glm::vec3 &planetCenter,
                                            float planetRadius)
    {
        // Direction from planet center to camera ("up" at the camera's location).
        glm::vec3 up = cameraPos - planetCenter;
        const float upLen = glm::length(up);
        if (upLen < 1e-5f)
        {
            // Degenerate (camera at planet center), fall back to world up.
            up = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        else
        {
            up /= upLen;
        }

        // Point on the sphere directly "below" the camera.
        const glm::vec3 surfacePoint = planetCenter + up * planetRadius;

        // Build an arbitrary orthonormal tangent basis at surfacePoint.
        glm::vec3 reference = glm::vec3(0.0f, 1.0f, 0.0f);
        if (std::abs(glm::dot(reference, up)) > 0.99f)
            reference = glm::vec3(1.0f, 0.0f, 0.0f);

        const glm::vec3 tangentU = glm::normalize(glm::cross(reference, up));
        const glm::vec3 tangentV = glm::normalize(glm::cross(up, tangentU));

#ifdef SF_OCEAN_DEBUG_LOG
        {
            static int s_logCount = 0;
            if (s_logCount < 5)
            {
                ++s_logCount;
                printf("[OceanDebug] cameraPos=(%.3f,%.3f,%.3f) upLen=%.6f planetCenter=(%.3f,%.3f,%.3f) planetRadius=%.6f\n",
                       cameraPos.x, cameraPos.y, cameraPos.z, upLen,
                       planetCenter.x, planetCenter.y, planetCenter.z, planetRadius);
                printf("[OceanDebug] up=(%.6f,%.6f,%.6f) surfacePoint=(%.3f,%.3f,%.3f)\n",
                       up.x, up.y, up.z, surfacePoint.x, surfacePoint.y, surfacePoint.z);
                printf("[OceanDebug] tangentU=(%.6f,%.6f,%.6f) tangentV=(%.6f,%.6f,%.6f)\n",
                       tangentU.x, tangentU.y, tangentU.z, tangentV.x, tangentV.y, tangentV.z);
                printf("[OceanDebug] patchExtent=(%.3f,%.3f) patchCount=%u distCamToSurface=%.6f\n",
                       patchExtent_.x, patchExtent_.y, patchCount_, glm::length(cameraPos - surfacePoint));
            }
        }
#endif

        writeVertices(surfacePoint, tangentU, tangentV, planetCenter, planetRadius);
    }

    void OceanTessellatedMesh::Draw(const CommandBuffer &cmd, uint32_t instanceCount) const
    {
        VkBuffer vb = vertexBuffer_->GetBuffer();
        VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cmd, indexBuffer_->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, indexCount_, instanceCount, 0, 0, 0);
    }

} // namespace SF::Engine