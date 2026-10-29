#pragma once

#include <Math/BasicMath.hpp>
#include <Rendering/RHI/Shaders/Shader.hpp>
#include <volk.h>

namespace SF::Engine
{
    /**
     * @breif Animation Joint data
     */
    struct JointData
    {
        Ui32Vec3 Id;
        Vec3 Weight;
    };

    /**
     * @brief Canonical engine vertex : position, normal, UV, tangent.
     * Matches the layout expected by standard engine shaders.
     * location 0 = position, 1 = normal, 2 = texCoord, 3 = tangent
     */
    struct Vertex
    {
        Vec3 position;
        Vec3 color;
        Vec3 texCoord; // 2d UV: u, v, 0
        Vec3 tangent;
        Vec3 normal;

        optional<JointData> jointData = {};

        static VkVertexInputBindingDescription GetBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding   = 0;
            bindingDescription.stride    = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 5> GetAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

            attributeDescriptions[0].binding  = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset   = offsetof(Vertex, position);

            attributeDescriptions[1].binding  = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset   = offsetof(Vertex, normal);

            attributeDescriptions[2].binding  = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format   = VK_FORMAT_R32G32B32_SFLOAT; // full UVW, not just UV
            attributeDescriptions[2].offset   = offsetof(Vertex, texCoord);

            attributeDescriptions[3].binding  = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format   = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[3].offset   = offsetof(Vertex, tangent);

            attributeDescriptions[4].binding  = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format   = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[4].offset   = offsetof(Vertex, color);

            return attributeDescriptions;
        }

        bool operator==(const Vertex &) const = default;

        static Shader::VertexInput GetVertexInput()
        {
            std::vector<VkVertexInputBindingDescription> bindings(1);
            bindings[0].binding   = 0;
            bindings[0].stride    = sizeof(Vertex);
            bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            std::vector<VkVertexInputAttributeDescription> attrs(4);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
            attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
            attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord)};
            attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)};

            return {std::move(bindings), std::move(attrs)};
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
            bindings[0].binding   = 0;
            bindings[0].stride    = sizeof(PatchVertex);
            bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            std::vector<VkVertexInputAttributeDescription> attrs(3);
            attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PatchVertex, position)};
            attrs[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(PatchVertex, uv)};
            attrs[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(PatchVertex, normal)};

            return {std::move(bindings), std::move(attrs)};
        }
    };
} // namespace SF::Engine
