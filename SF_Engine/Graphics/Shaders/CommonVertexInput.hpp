#pragma once
#include "Shader.hpp"
#include <glm/glm.hpp>

namespace SF::Engine
{
    /**
     * @brief Simple 2D vertex with position and UV coordinates
     */
    class Vertex2D : public Shader::VertexInput
    {
    public:
        glm::vec2 position;
        glm::vec2 texCoord;

        Vertex2D() = default;
        Vertex2D(const glm::vec2 &pos, const glm::vec2 &uv)
            : position(pos), texCoord(uv) {}

        std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() const override
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(Vertex2D);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return {binding};
        }

        std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() const override
        {
            std::vector<VkVertexInputAttributeDescription> attributes(2);

            // Position
            attributes[0].binding = 0;
            attributes[0].location = 0;
            attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[0].offset = offsetof(Vertex2D, position);

            // TexCoord
            attributes[1].binding = 0;
            attributes[1].location = 1;
            attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[1].offset = offsetof(Vertex2D, texCoord);

            return attributes;
        }
    };

    /**
     * @brief Standard 3D vertex with position, normal, and UV coordinates
     */
    class Vertex3D : public Shader::VertexInput
    {
    public:
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;

        Vertex3D() = default;
        Vertex3D(const glm::vec3 &pos, const glm::vec3 &norm, const glm::vec2 &uv)
            : position(pos), normal(norm), texCoord(uv) {}

        std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() const override
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(Vertex3D);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return {binding};
        }

        std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() const override
        {
            std::vector<VkVertexInputAttributeDescription> attributes(3);

            // Position
            attributes[0].binding = 0;
            attributes[0].location = 0;
            attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[0].offset = offsetof(Vertex3D, position);

            // Normal
            attributes[1].binding = 0;
            attributes[1].location = 1;
            attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[1].offset = offsetof(Vertex3D, normal);

            // TexCoord
            attributes[2].binding = 0;
            attributes[2].location = 2;
            attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[2].offset = offsetof(Vertex3D, texCoord);

            return attributes;
        }
    };

    /**
     * @brief Extended 3D vertex with tangent and bitangent for normal mapping
     */
    class VertexPBR : public Shader::VertexInput
    {
    public:
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec3 tangent;
        glm::vec3 bitangent;

        VertexPBR() = default;

        std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() const override
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(VertexPBR);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return {binding};
        }

        std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() const override
        {
            std::vector<VkVertexInputAttributeDescription> attributes(5);

            // Position
            attributes[0].binding = 0;
            attributes[0].location = 0;
            attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[0].offset = offsetof(VertexPBR, position);

            // Normal
            attributes[1].binding = 0;
            attributes[1].location = 1;
            attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[1].offset = offsetof(VertexPBR, normal);

            // TexCoord
            attributes[2].binding = 0;
            attributes[2].location = 2;
            attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[2].offset = offsetof(VertexPBR, texCoord);

            // Tangent
            attributes[3].binding = 0;
            attributes[3].location = 3;
            attributes[3].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[3].offset = offsetof(VertexPBR, tangent);

            // Bitangent
            attributes[4].binding = 0;
            attributes[4].location = 4;
            attributes[4].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[4].offset = offsetof(VertexPBR, bitangent);

            return attributes;
        }
    };

    /**
     * @brief Colored vertex for simple rendering without textures
     */
    class VertexColored : public Shader::VertexInput
    {
    public:
        glm::vec3 position;
        glm::vec4 color;

        VertexColored() = default;
        VertexColored(const glm::vec3 &pos, const glm::vec4 &col)
            : position(pos), color(col) {}

        std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() const override
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(VertexColored);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return {binding};
        }

        std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() const override
        {
            std::vector<VkVertexInputAttributeDescription> attributes(2);

            // Position
            attributes[0].binding = 0;
            attributes[0].location = 0;
            attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[0].offset = offsetof(VertexColored, position);

            // Color
            attributes[1].binding = 0;
            attributes[1].location = 1;
            attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributes[1].offset = offsetof(VertexColored, color);

            return attributes;
        }
    };

    /**
     * @brief Instanced vertex data (for instanced rendering)
     */
    class VertexInstance : public Shader::VertexInput
    {
    public:
        glm::mat4 modelMatrix;
        glm::vec4 color;

        VertexInstance() = default;

        std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() const override
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 1; // Use binding 1 for instance data
            binding.stride = sizeof(VertexInstance);
            binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
            return {binding};
        }

        std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() const override
        {
            std::vector<VkVertexInputAttributeDescription> attributes(5);

            // Model matrix (4 vec4s, locations 5-8)
            for (uint32_t i = 0; i < 4; i++)
            {
                attributes[i].binding = 1;
                attributes[i].location = 5 + i;
                attributes[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
                attributes[i].offset = offsetof(VertexInstance, modelMatrix) + sizeof(glm::vec4) * i;
            }

            // Color
            attributes[4].binding = 1;
            attributes[4].location = 9;
            attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributes[4].offset = offsetof(VertexInstance, color);

            return attributes;
        }
    };

    /**
     * @brief UI/GUI vertex with position, UV, and color
     */
    class VertexUI : public Shader::VertexInput
    {
    public:
        glm::vec2 position;
        glm::vec2 texCoord;
        glm::vec4 color;

        VertexUI() = default;
        VertexUI(const glm::vec2 &pos, const glm::vec2 &uv, const glm::vec4 &col)
            : position(pos), texCoord(uv), color(col) {}

        std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() const override
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(VertexUI);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return {binding};
        }

        std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() const override
        {
            std::vector<VkVertexInputAttributeDescription> attributes(3);

            // Position
            attributes[0].binding = 0;
            attributes[0].location = 0;
            attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[0].offset = offsetof(VertexUI, position);

            // TexCoord
            attributes[1].binding = 0;
            attributes[1].location = 1;
            attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[1].offset = offsetof(VertexUI, texCoord);

            // Color
            attributes[2].binding = 0;
            attributes[2].location = 2;
            attributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributes[2].offset = offsetof(VertexUI, color);

            return attributes;
        }
    };

    /**
     * @brief Skinned mesh vertex with bone weights and indices
     */
    class VertexSkinned : public Shader::VertexInput
    {
    public:
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoord;
        glm::vec4 boneWeights;
        glm::ivec4 boneIndices;

        VertexSkinned() = default;

        std::vector<VkVertexInputBindingDescription> GetBindingDescriptions() const override
        {
            VkVertexInputBindingDescription binding{};
            binding.binding = 0;
            binding.stride = sizeof(VertexSkinned);
            binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return {binding};
        }

        std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions() const override
        {
            std::vector<VkVertexInputAttributeDescription> attributes(5);

            // Position
            attributes[0].binding = 0;
            attributes[0].location = 0;
            attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[0].offset = offsetof(VertexSkinned, position);

            // Normal
            attributes[1].binding = 0;
            attributes[1].location = 1;
            attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributes[1].offset = offsetof(VertexSkinned, normal);

            // TexCoord
            attributes[2].binding = 0;
            attributes[2].location = 2;
            attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributes[2].offset = offsetof(VertexSkinned, texCoord);

            // Bone Weights
            attributes[3].binding = 0;
            attributes[3].location = 3;
            attributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attributes[3].offset = offsetof(VertexSkinned, boneWeights);

            // Bone Indices
            attributes[4].binding = 0;
            attributes[4].location = 4;
            attributes[4].format = VK_FORMAT_R32G32B32A32_SINT;
            attributes[4].offset = offsetof(VertexSkinned, boneIndices);

            return attributes;
        }
    };
}