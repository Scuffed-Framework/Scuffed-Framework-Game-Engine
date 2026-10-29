#pragma once

#include <Math/BasicMath.hpp>
#include <memory>
#include <numbers>
#include "Mesh.hpp"

namespace SF::Engine::MeshFactory
{
    /** @brief Unit cube (1x1x1, centred at origin). 24 vertices, per-face normals. */
    inline std::unique_ptr<Mesh> CreateCube()
    {
        const std::vector<Vertex> verts = {
                // Front (+Z)
                MakeVertex({-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {0, 0}, {1, 0, 0}),
                MakeVertex({0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 0}, {1, 0, 0}),
                MakeVertex({0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 1}, {1, 0, 0}),
                MakeVertex({-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0, 1}, {1, 0, 0}),
                // Back (-Z)
                MakeVertex({0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 0}, {-1, 0, 0}),
                MakeVertex({-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 0}, {-1, 0, 0}),
                MakeVertex({-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {1, 1}, {-1, 0, 0}),
                MakeVertex({0.5f, 0.5f, -0.5f}, {0, 0, -1}, {0, 1}, {-1, 0, 0}),
                // Left (-X)
                MakeVertex({-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}, {0, 0, 1}),
                MakeVertex({-0.5f, -0.5f, 0.5f}, {-1, 0, 0}, {1, 0}, {0, 0, 1}),
                MakeVertex({-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1, 1}, {0, 0, 1}),
                MakeVertex({-0.5f, 0.5f, -0.5f}, {-1, 0, 0}, {0, 1}, {0, 0, 1}),
                // Right (+X)
                MakeVertex({0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 0}, {0, 0, -1}),
                MakeVertex({0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 0}, {0, 0, -1}),
                MakeVertex({0.5f, 0.5f, -0.5f}, {1, 0, 0}, {1, 1}, {0, 0, -1}),
                MakeVertex({0.5f, 0.5f, 0.5f}, {1, 0, 0}, {0, 1}, {0, 0, -1}),
                // Top (+Y)
                MakeVertex({-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {0, 0}, {1, 0, 0}),
                MakeVertex({0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 0}, {1, 0, 0}),
                MakeVertex({0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 1}, {1, 0, 0}),
                MakeVertex({-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {0, 1}, {1, 0, 0}),
                // Bottom (-Y)
                MakeVertex({-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 0}, {1, 0, 0}),
                MakeVertex({0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 0}, {1, 0, 0}),
                MakeVertex({0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 1}, {1, 0, 0}),
                MakeVertex({-0.5f, -0.5f, 0.5f}, {0, -1, 0}, {0, 1}, {1, 0, 0}),
        };

        const std::vector<uint32_t> idx = {
                0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
                12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20,
        };

        return std::make_unique<Mesh>(verts, idx);
    }

    /** @brief Unit quad on the XY plane, facing +Z. */
    inline std::unique_ptr<Mesh> CreateQuad()
    {
        const std::vector<Vertex> verts = {
                MakeVertex({-0.5f, -0.5f, 0}, {0, 0, 1}, {0, 0}, {1, 0, 0}),
                MakeVertex({0.5f, -0.5f, 0}, {0, 0, 1}, {1, 0}, {1, 0, 0}),
                MakeVertex({0.5f, 0.5f, 0}, {0, 0, 1}, {1, 1}, {1, 0, 0}),
                MakeVertex({-0.5f, 0.5f, 0}, {0, 0, 1}, {0, 1}, {1, 0, 0}),
        };
        const std::vector<uint32_t> idx = {0, 1, 2, 2, 3, 0};
        return std::make_unique<Mesh>(verts, idx);
    }

    /**
     * @brief UV sphere.
     * @param stacks  Latitude subdivisions (min 2).
     * @param slices  Longitude subdivisions (min 3).
     */
    inline std::unique_ptr<Mesh> CreateSphere(uint32_t stacks = 16, uint32_t slices = 32)
    {
        std::vector<Vertex> verts;
        std::vector<uint32_t> idx;

        for (uint32_t i = 0; i <= stacks; ++i)
        {
            float phi = std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(stacks);
            for (uint32_t j = 0; j <= slices; ++j)
            {
                const float theta =
                        2.0f * std::numbers::pi_v<float> * static_cast<float>(j) / static_cast<float>(slices);
                Vec3 pos = {std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)};
                Vec2 uv  = {static_cast<float>(j) / static_cast<float>(slices),
                            static_cast<float>(i) / static_cast<float>(stacks)};
                Vec3 tan = {-std::sin(theta), 0.0f, std::cos(theta)};
                verts.push_back(MakeVertex(pos, normalize(pos), uv, tan));
            }
        }

        for (uint32_t i = 0; i < stacks; ++i)
        {
            for (uint32_t j = 0; j < slices; ++j)
            {
                uint32_t a = i * (slices + 1) + j;
                uint32_t b = a + slices + 1;
                idx.insert(idx.end(), {a, b, a + 1, b, b + 1, a + 1});
            }
        }

        return std::make_unique<Mesh>(verts, idx);
    }
} // namespace SF::Engine::MeshFactory
