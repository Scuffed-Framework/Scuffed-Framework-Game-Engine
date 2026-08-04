#pragma once

#define VK_NO_PROTOTYPES

#include <vulkan/vulkan.h>
#include <array>
#include <Math/Vectors/Vector3.hpp>
#include <vector>

namespace SF::Engine
{
    struct Vertex
    {
        Vector3float pos;
        Vector3float color;
        Vector3float texCoord;  // UV
        Vector3float tangent;
        Vector3float normal;

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

        inline Vector3float lerpVec3(const Vector3float& a, const Vector3float& b, float t)
        {
            return Vector3float{
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t
            };
        }

        inline Vector3float scaleVec3(const Vector3float& v, float s)
        {
            return Vector3float{ v.x * s, v.y * s, v.z * s };
        }

        inline Vector3float normalizeVec3(const Vector3float& v)
        {
            float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            if (len < 1e-8f)
                return v;
            return Vector3float{ v.x / len, v.y / len, v.z / len };
        }

        inline Vector3float crossVec3(const Vector3float& a, const Vector3float& b)
        {
            return Vector3float{
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        // Spherical UV from a *normalized* position
        inline Vector3float sphericalUV(const Vector3float& n)
        {
            constexpr float kPi = 3.14159265358979323846f;
            float u = 0.5f + std::atan2(n.z, n.x) / (2.0f * kPi);
            float v = 0.5f - std::asin(std::clamp(n.y, -1.0f, 1.0f)) / kPi;
            return Vector3float{ u, v, 0.0f };
        }

        // Tangent perpendicular to the normal (degenerates gracefully at the poles)
        inline Vector3float tangentFromNormal(const Vector3float& n)
        {
            Vector3float up{ 0.0f, 1.0f, 0.0f };
            if (std::abs(n.y) > 0.999f)
                up = Vector3float{ 1.0f, 0.0f, 0.0f };
            return normalizeVec3(crossVec3(up, n));
        }

        // Midpoint of two vertices, pushed out onto the sphere of the given radius
        inline Vertex midpointVertex(const Vertex& a, const Vertex& b, float radius)
        {
            Vector3float posMid = lerpVec3(a.pos, b.pos, 0.5f);
            Vector3float n       = normalizeVec3(posMid);

            Vertex out{};
            out.pos      = scaleVec3(n, radius);
            out.normal   = n;
            out.tangent  = tangentFromNormal(n);
            out.color    = lerpVec3(a.color, b.color, 0.5f);
            out.texCoord = sphericalUV(n);
            return out;
        }

        /** @brief Recursively subdivide a triangle, pushing flat (non-indexed) vertex triples.
        * re-projects new midpoints onto the sphere at each step, which is what turns an icosahedron into an icosphere.
        */
        inline void SubdivideTriangle(std::vector<Vertex>& vertices, Vertex A, Vertex B, Vertex C,
                                    uint8_t repetitions, float radius = 1.0f)
        {
            if (repetitions == 0)
            {
                vertices.push_back(A);
                vertices.push_back(B);
                vertices.push_back(C);
                return;
            }

            Vertex AB = midpointVertex(A, B, radius);
            Vertex BC = midpointVertex(B, C, radius);
            Vertex CA = midpointVertex(C, A, radius);

            SubdivideTriangle(vertices, A,  AB, CA, repetitions - 1, radius);
            SubdivideTriangle(vertices, B,  BC, AB, repetitions - 1, radius);
            SubdivideTriangle(vertices, C,  CA, BC, repetitions - 1, radius);
            SubdivideTriangle(vertices, AB, BC, CA, repetitions - 1, radius); // center triangle
        }

        constexpr size_t icosphereVertexCount(uint8_t subdivisions)
        {
            size_t count = 60;
            for (uint8_t i = 0; i < subdivisions; ++i)
                count *= 4;
            return count;
        }

        inline Vertex makeIcoVertex(const Vector3float& rawPos, float radius)
        {
            Vector3float n = normalizeVec3(rawPos);
            Vertex v{};
            v.pos      = scaleVec3(n, radius);
            v.normal   = n;
            v.tangent  = tangentFromNormal(n);
            v.color    = Vector3float{ 1.0f, 1.0f, 1.0f };
            v.texCoord = sphericalUV(n);
            return v;
        }

        template <uint8_t Subdivisions>
        inline Shape<icosphereVertexCount(Subdivisions)> createIcosphere(float radius = 0.5f)
        {
            constexpr float t = 1.61803398875f; // golden ratio

            // 12 base icosahedron positions (unnormalized)
            std::array<Vector3float, 12> raw = {{
                {-1,  t,  0}, { 1,  t,  0}, {-1, -t,  0}, { 1, -t,  0},
                { 0, -1,  t}, { 0,  1,  t}, { 0, -1, -t}, { 0,  1, -t},
                { t,  0, -1}, { t,  0,  1}, {-t,  0, -1}, {-t,  0,  1}
            }};

            std::array<Vertex, 12> baseVerts{};
            for (size_t i = 0; i < 12; ++i)
                baseVerts[i] = makeIcoVertex(raw[i], radius);

            // 20 faces of the icosahedron
            static constexpr int faces[20][3] = {
                {0,11,5}, {0,5,1},  {0,1,7},  {0,7,10}, {0,10,11},
                {1,5,9},  {5,11,4}, {11,10,2},{10,7,6}, {7,1,8},
                {3,9,4},  {3,4,2},  {3,2,6},  {3,6,8},  {3,8,9},
                {4,9,5},  {2,4,11}, {6,2,10}, {8,6,7},  {9,8,1}
            };

            std::vector<Vertex> vertices;
            vertices.reserve(icosphereVertexCount(Subdivisions));

            for (const auto& f : faces)
            {
                SubdivideTriangle(vertices, baseVerts[f[0]], baseVerts[f[1]], baseVerts[f[2]],
                                Subdivisions, radius);
            }

            std::array<Vertex, icosphereVertexCount(Subdivisions)> arr{};
            std::copy(vertices.begin(), vertices.end(), arr.begin());

            return Shape<icosphereVertexCount(Subdivisions)>(arr);
        }
    }
}