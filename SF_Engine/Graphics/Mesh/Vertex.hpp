#pragma once

#include <Graphics/Shaders/Shader.hpp>
#include <glm/glm.hpp>
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
        glm::vec3 position = {};
        glm::vec3 normal = {};
        glm::vec2 texCoord = {};
        glm::vec3 tangent = {};

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
}
