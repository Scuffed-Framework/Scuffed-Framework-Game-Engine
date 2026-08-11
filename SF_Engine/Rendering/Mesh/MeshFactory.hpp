#pragma once

#include "Mesh.hpp"
#include <memory>
#include <numbers>
#include <Math/BasicMath.hpp>

namespace SF::Engine::MeshFactory
{
    /**
     * @brief Unit cube (1x1x1, centred at origin). 24 vertices, per-face normals.
     */
    inline std::unique_ptr<Mesh> CreateCube()
    {
        // 4 verts per face, 6 faces = 24 verts
        const std::vector<Vertex> verts = {
            // Front (+Z)
            {{-0.5f,-0.5f, 0.5f},{0,0,1},{0,0},{1,0,0}},
            {{ 0.5f,-0.5f, 0.5f},{0,0,1},{1,0},{1,0,0}},
            {{ 0.5f, 0.5f, 0.5f},{0,0,1},{1,1},{1,0,0}},
            {{-0.5f, 0.5f, 0.5f},{0,0,1},{0,1},{1,0,0}},
            // Back (-Z)
            {{ 0.5f,-0.5f,-0.5f},{0,0,-1},{0,0},{-1,0,0}},
            {{-0.5f,-0.5f,-0.5f},{0,0,-1},{1,0},{-1,0,0}},
            {{-0.5f, 0.5f,-0.5f},{0,0,-1},{1,1},{-1,0,0}},
            {{ 0.5f, 0.5f,-0.5f},{0,0,-1},{0,1},{-1,0,0}},
            // Left (-X)
            {{-0.5f,-0.5f,-0.5f},{-1,0,0},{0,0},{0,0,1}},
            {{-0.5f,-0.5f, 0.5f},{-1,0,0},{1,0},{0,0,1}},
            {{-0.5f, 0.5f, 0.5f},{-1,0,0},{1,1},{0,0,1}},
            {{-0.5f, 0.5f,-0.5f},{-1,0,0},{0,1},{0,0,1}},
            // Right (+X)
            {{ 0.5f,-0.5f, 0.5f},{1,0,0},{0,0},{0,0,-1}},
            {{ 0.5f,-0.5f,-0.5f},{1,0,0},{1,0},{0,0,-1}},
            {{ 0.5f, 0.5f,-0.5f},{1,0,0},{1,1},{0,0,-1}},
            {{ 0.5f, 0.5f, 0.5f},{1,0,0},{0,1},{0,0,-1}},
            // Top (+Y)
            {{-0.5f, 0.5f, 0.5f},{0,1,0},{0,0},{1,0,0}},
            {{ 0.5f, 0.5f, 0.5f},{0,1,0},{1,0},{1,0,0}},
            {{ 0.5f, 0.5f,-0.5f},{0,1,0},{1,1},{1,0,0}},
            {{-0.5f, 0.5f,-0.5f},{0,1,0},{0,1},{1,0,0}},
            // Bottom (-Y)
            {{-0.5f,-0.5f,-0.5f},{0,-1,0},{0,0},{1,0,0}},
            {{ 0.5f,-0.5f,-0.5f},{0,-1,0},{1,0},{1,0,0}},
            {{ 0.5f,-0.5f, 0.5f},{0,-1,0},{1,1},{1,0,0}},
            {{-0.5f,-0.5f, 0.5f},{0,-1,0},{0,1},{1,0,0}},
        };

        const std::vector<uint32_t> idx = {
             0, 1, 2,  2, 3, 0,
             4, 5, 6,  6, 7, 4,
             8, 9,10, 10,11, 8,
            12,13,14, 14,15,12,
            16,17,18, 18,19,16,
            20,21,22, 22,23,20,
        };

        return std::make_unique<Mesh>(verts, idx);
    }

    /**
     * @brief Unit quad on the XY plane, facing +Z.
     */
    inline std::unique_ptr<Mesh> CreateQuad()
    {
        const std::vector<Vertex> verts = {
            {{-0.5f,-0.5f,0},{0,0,1},{0,0},{1,0,0}},
            {{ 0.5f,-0.5f,0},{0,0,1},{1,0},{1,0,0}},
            {{ 0.5f, 0.5f,0},{0,0,1},{1,1},{1,0,0}},
            {{-0.5f, 0.5f,0},{0,0,1},{0,1},{1,0,0}},
        };
        const std::vector<uint32_t> idx = {0,1,2, 2,3,0};
        return std::make_unique<Mesh>(verts, idx);
    }

    /**
     * @brief UV sphere.
     * @param stacks  Latitude subdivisions (min 2).
     * @param slices  Longitude subdivisions (min 3).
     */
    inline std::unique_ptr<Mesh> CreateSphere(uint32_t stacks = 16, uint32_t slices = 32)
    {
        std::vector<Vertex>   verts;
        std::vector<uint32_t> idx;

        for (uint32_t i = 0; i <= stacks; ++i)
        {
            float phi = std::numbers::pi_v<float> * float(i) / float(stacks);
            for (uint32_t j = 0; j <= slices; ++j)
            {
                float theta = 2.0f * std::numbers::pi_v<float> * float(j) / float(slices);
                Vec3 pos = {
                    std::sin(phi) * std::cos(theta),
                    std::cos(phi),
                    std::sin(phi) * std::sin(theta)
                };
                Vec2 uv  = {float(j)/float(slices), float(i)/float(stacks)};
                Vec3 tan = {-std::sin(theta), 0, std::cos(theta)};
                verts.push_back({pos, normalize(pos), uv, tan});
            }
        }

        for (uint32_t i = 0; i < stacks; ++i)
        {
            for (uint32_t j = 0; j < slices; ++j)
            {
                uint32_t a = i * (slices + 1) + j;
                uint32_t b = a + slices + 1;
                idx.insert(idx.end(), {a, b, a+1, b, b+1, a+1});
            }
        }

        return std::make_unique<Mesh>(verts, idx);
    }
}
