#pragma once

#define VK_NO_PROTOTYPES

#include <vulkan/vulkan.h>
#include <array>
#include <glm/glm.hpp>
#include <vector>

namespace SF::Engine
{
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;  // UV
        glm::vec3 tangent;
        glm::vec3 normal;

        static VkVertexInputBindingDescription getBindingDescription()
        {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 5> getAttributeDescriptions()
        {
            std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, normal);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

            attributeDescriptions[3].binding = 0;
            attributeDescriptions[3].location = 3;
            attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[3].offset = offsetof(Vertex, tangent);

            attributeDescriptions[4].binding = 0;
            attributeDescriptions[4].location = 4;
            attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[4].offset = offsetof(Vertex, color);

            return attributeDescriptions;
        }
    };

    // Uniform buffer object for MVP matrices
    struct UniformBufferObject
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };

    // Camera data for shaders
    struct CameraData
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 viewProj;
        alignas(16) glm::vec4 cameraPos;
        alignas(16) glm::vec4 screenDimensions;  // width, height, nearPlane, farPlane
    };

    // Scene-level uniform data
    struct SceneUBO
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 projection;
        alignas(16) glm::vec3 cameraPos;
        float _padding;
    };

    // Data passed between shader stages
    struct PerStageData
    {
        glm::vec3 worldPos;
        glm::vec2 texCoord;
        glm::vec3 worldNormal;
        glm::vec3 worldTangent;
        glm::vec3 worldBitangent;
    };

    // Matches Light struct in GLSL
    struct Light
    {
        alignas(16) glm::vec4 position;  // .w = type (0=point, 1=directional, 2=spot)
        alignas(16) glm::vec4 color;     // .w = intensity
        alignas(16) glm::vec4 params;    // x: range, y: radius, z: spotAngle, w: spotBlend
    };

    // Matches UBO_Lights in GLSL
    struct LightsUBO
    {
        Light lights[16];
        alignas(16) int lightCount;
        int _padding[3];  // Ensure 16-byte alignment
    };

    // Shape template with proper indices support
    template <size_t VertexCount, typename IndexType = uint32_t>
    class Shape
    {
    public:
        std::array<Vertex, VertexCount> vertices;
        std::vector<IndexType> indices;

        Shape() = default;

        // Initialize with vertices only (no indices)
        Shape(const std::array<Vertex, VertexCount>& verts) : vertices(verts) {}

        // Initialize with vertices and indices
        Shape(const std::array<Vertex, VertexCount>& verts, const std::vector<IndexType>& idxs)
            : vertices(verts), indices(idxs)
        {
        }

        // Get vertex data pointer
        const void* getVertexData() const
        {
            return vertices.data();
        }

        // Get vertex data size in bytes
        size_t getVertexDataSize() const
        {
            return sizeof(Vertex) * VertexCount;
        }

        // Get index data pointer
        const void* getIndexData() const
        {
            return indices.data();
        }

        // Get index data size in bytes
        size_t getIndexDataSize() const
        {
            return sizeof(IndexType) * indices.size();
        }

        // Get vertex count
        size_t getVertexCount() const
        {
            return VertexCount;
        }

        // Get index count
        size_t getIndexCount() const
        {
            return indices.size();
        }

        // Check if shape uses indices
        bool hasIndices() const
        {
            return !indices.empty();
        }

        // Get index type for Vulkan
        VkIndexType getIndexType() const
        {
            if constexpr (sizeof(IndexType) == sizeof(uint16_t))
                return VK_INDEX_TYPE_UINT16;
            else
                return VK_INDEX_TYPE_UINT32;
        }
    };

    // Helper functions to create common shapes
    namespace ShapeFactory
    {
        // Create a triangle
        inline Shape<3> createTriangle()
        {
            std::array<Vertex, 3> vertices = {{{{0.0f, -0.5f, 0.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.5f, 0.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f}},
                                               {{0.5f, 0.5f, 0.0f},
                                                {0.0f, 1.0f, 0.0f},
                                                {1.0f, 1.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f}},
                                               {{-0.5f, 0.5f, 0.0f},
                                                {0.0f, 0.0f, 1.0f},
                                                {0.0f, 1.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f}}}};
            return Shape<3>(vertices);
        }

        // Create a quad with indices
        inline Shape<4> createQuad()
        {
            std::array<Vertex, 4> vertices = {{{{-0.5f, -0.5f, 0.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f}},
                                               {{0.5f, -0.5f, 0.0f},
                                                {0.0f, 1.0f, 0.0f},
                                                {1.0f, 0.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f}},
                                               {{0.5f, 0.5f, 0.0f},
                                                {0.0f, 0.0f, 1.0f},
                                                {1.0f, 1.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f}},
                                               {{-0.5f, 0.5f, 0.0f},
                                                {1.0f, 1.0f, 0.0f},
                                                {0.0f, 1.0f},
                                                {1.0f, 0.0f, 0.0f},
                                                {0.0f, 0.0f, 1.0f}}}};

            std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0};

            return Shape<4>(vertices, indices);
        }

        // Create a cube with indices (24 vertices for proper normals per face)
        inline Shape<24> createCube()
        {
            std::array<Vertex, 24> vertices = {{// Front face (normal: 0, 0, 1)
                                                {{-0.5f, -0.5f, 0.5f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, 1.0f}},
                                                {{0.5f, -0.5f, 0.5f},
                                                 {0.0f, 1.0f, 0.0f},
                                                 {1.0f, 0.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, 1.0f}},
                                                {{0.5f, 0.5f, 0.5f},
                                                 {0.0f, 0.0f, 1.0f},
                                                 {1.0f, 1.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, 1.0f}},
                                                {{-0.5f, 0.5f, 0.5f},
                                                 {1.0f, 1.0f, 0.0f},
                                                 {0.0f, 1.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, 1.0f}},

                                                // Back face (normal: 0, 0, -1)
                                                {{0.5f, -0.5f, -0.5f},
                                                 {1.0f, 0.0f, 1.0f},
                                                 {0.0f, 0.0f},
                                                 {-1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, -1.0f}},
                                                {{-0.5f, -0.5f, -0.5f},
                                                 {0.0f, 1.0f, 1.0f},
                                                 {1.0f, 0.0f},
                                                 {-1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, -1.0f}},
                                                {{-0.5f, 0.5f, -0.5f},
                                                 {1.0f, 1.0f, 1.0f},
                                                 {1.0f, 1.0f},
                                                 {-1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, -1.0f}},
                                                {{0.5f, 0.5f, -0.5f},
                                                 {0.5f, 0.5f, 0.5f},
                                                 {0.0f, 1.0f},
                                                 {-1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f, -1.0f}},

                                                // Left face (normal: -1, 0, 0)
                                                {{-0.5f, -0.5f, -0.5f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f},
                                                 {0.0f, 0.0f, 1.0f},
                                                 {-1.0f, 0.0f, 0.0f}},
                                                {{-0.5f, -0.5f, 0.5f},
                                                 {0.0f, 1.0f, 0.0f},
                                                 {1.0f, 0.0f},
                                                 {0.0f, 0.0f, 1.0f},
                                                 {-1.0f, 0.0f, 0.0f}},
                                                {{-0.5f, 0.5f, 0.5f},
                                                 {0.0f, 0.0f, 1.0f},
                                                 {1.0f, 1.0f},
                                                 {0.0f, 0.0f, 1.0f},
                                                 {-1.0f, 0.0f, 0.0f}},
                                                {{-0.5f, 0.5f, -0.5f},
                                                 {1.0f, 1.0f, 0.0f},
                                                 {0.0f, 1.0f},
                                                 {0.0f, 0.0f, 1.0f},
                                                 {-1.0f, 0.0f, 0.0f}},

                                                // Right face (normal: 1, 0, 0)
                                                {{0.5f, -0.5f, 0.5f},
                                                 {1.0f, 0.0f, 1.0f},
                                                 {0.0f, 0.0f},
                                                 {0.0f, 0.0f, -1.0f},
                                                 {1.0f, 0.0f, 0.0f}},
                                                {{0.5f, -0.5f, -0.5f},
                                                 {0.0f, 1.0f, 1.0f},
                                                 {1.0f, 0.0f},
                                                 {0.0f, 0.0f, -1.0f},
                                                 {1.0f, 0.0f, 0.0f}},
                                                {{0.5f, 0.5f, -0.5f},
                                                 {1.0f, 1.0f, 1.0f},
                                                 {1.0f, 1.0f},
                                                 {0.0f, 0.0f, -1.0f},
                                                 {1.0f, 0.0f, 0.0f}},
                                                {{0.5f, 0.5f, 0.5f},
                                                 {0.5f, 0.5f, 0.5f},
                                                 {0.0f, 1.0f},
                                                 {0.0f, 0.0f, -1.0f},
                                                 {1.0f, 0.0f, 0.0f}},

                                                // Top face (normal: 0, 1, 0)
                                                {{-0.5f, 0.5f, 0.5f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 0.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 1.0f, 0.0f}},
                                                {{0.5f, 0.5f, 0.5f},
                                                 {0.0f, 1.0f, 0.0f},
                                                 {1.0f, 0.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 1.0f, 0.0f}},
                                                {{0.5f, 0.5f, -0.5f},
                                                 {0.0f, 0.0f, 1.0f},
                                                 {1.0f, 1.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 1.0f, 0.0f}},
                                                {{-0.5f, 0.5f, -0.5f},
                                                 {1.0f, 1.0f, 0.0f},
                                                 {0.0f, 1.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, 1.0f, 0.0f}},

                                                // Bottom face (normal: 0, -1, 0)
                                                {{-0.5f, -0.5f, -0.5f},
                                                 {1.0f, 0.0f, 1.0f},
                                                 {0.0f, 0.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, -1.0f, 0.0f}},
                                                {{0.5f, -0.5f, -0.5f},
                                                 {0.0f, 1.0f, 1.0f},
                                                 {1.0f, 0.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, -1.0f, 0.0f}},
                                                {{0.5f, -0.5f, 0.5f},
                                                 {1.0f, 1.0f, 1.0f},
                                                 {1.0f, 1.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, -1.0f, 0.0f}},
                                                {{-0.5f, -0.5f, 0.5f},
                                                 {0.5f, 0.5f, 0.5f},
                                                 {0.0f, 1.0f},
                                                 {1.0f, 0.0f, 0.0f},
                                                 {0.0f, -1.0f, 0.0f}}}};

            std::vector<uint32_t> indices = {
                0,  1,  2,  2,  3,  0,   // Front
                4,  5,  6,  6,  7,  4,   // Back
                8,  9,  10, 10, 11, 8,   // Left
                12, 13, 14, 14, 15, 12,  // Right
                16, 17, 18, 18, 19, 16,  // Top
                20, 21, 22, 22, 23, 20   // Bottom
            };

            return Shape<24>(vertices, indices);
        }
    }
}