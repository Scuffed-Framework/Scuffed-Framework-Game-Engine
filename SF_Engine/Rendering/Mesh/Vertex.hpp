#pragma once

#include <Rendering/Shaders/Shader.hpp>
#include <Math/BasicMath.hpp>
#include <volk.h>

namespace SF::Engine
{
    /**
     * @brief Canonical engine vertex : position, normal, UV, tangent.
     * Matches the layout expected by standard engine shaders.
     * location 0 = position, 1 = normal, 2 = texCoord, 3 = tangent
     */
    struct Vertex
    {
        Vec3 position = {};
        Vec3 normal = {};
        Vec2 texCoord = {};
        Vec3 tangent = {};

        bool operator==(const Vertex &) const = default;

        static Shader::VertexInput GetVertexInput()
        {
            std::vector<VkVertexInputBindingDescription> bindings(1);
            bindings[0].binding = 0;
            bindings[0].stride = sizeof(Vertex);
            bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            std::vector<VkVertexInputAttributeDescription> attrs(4);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
            attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
            attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord)};
            attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)};

            return Shader::VertexInput(std::move(bindings), std::move(attrs));
        }
    };
    struct PatchVertex
    {
        Vec3 position;
        Vec2 uv;
        Vec3 normal;

        static Shader::VertexInput GetVertexInput()
        {
            std::vector<VkVertexInputBindingDescription> bindings(1);
            bindings[0].binding = 0;
            bindings[0].stride = sizeof(PatchVertex);
            bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            std::vector<VkVertexInputAttributeDescription> attrs(3);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PatchVertex, position)};
            attrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(PatchVertex, uv)};
            attrs[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PatchVertex, normal)};

            return Shader::VertexInput(std::move(bindings), std::move(attrs));
        }
    };
}
