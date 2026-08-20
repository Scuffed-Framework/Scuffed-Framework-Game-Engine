#pragma once

#include <cstring>
#include <functional>
#include <unordered_map>

#include <Math/BasicMath.hpp>
#include <Rendering/Buffers/Buffer.hpp>
#include <Rendering/Mesh/Vertex.hpp>

// Cool feature idea:
// import whole .blend files and set up meshes, cameras, and lights.
// to add: usdz and other model types.
namespace SF::Engine
{
    template <typename Base>
    class ModelFactory
    {
    public:
        using TCreateReturn = std::shared_ptr<Base>;

        using TCreateMethodFilename = std::function<TCreateReturn(const std::filesystem::path &)>;
        using TRegistryMapFilename = std::unordered_map<std::string, TCreateMethodFilename>;

        virtual ~ModelFactory() = default;

        /**
         * Creates a new model, or finds one with the same values.
         * @param filename The file to load the model from.
         * @return The model loaded from the filename.
         */
        static TCreateReturn Create(const std::filesystem::path &filename)
        {
            auto fileExt = filename.extension().string();
            auto it = RegistryFilename().find(fileExt);
            return it == RegistryFilename().end() ? nullptr : it->second(filename);
        }

        static TRegistryMapFilename &RegistryFilename()
        {
            static TRegistryMapFilename impl;
            return impl;
        }

        template <typename T>
        class Registrar : public Base
        {
        protected:
            template <int Dummy = 0>
            static bool Register(const std::string &typeName, const std::string &extension)
            {
                ModelFactory::RegistryFilename()[extension] = [](const std::filesystem::path &filename) -> TCreateReturn
                {
                    return T::Create(filename);
                };
                return true;
            }

            inline static std::string name;
        };
    };

    /**
     * @brief Resource that represents a model vertex and index buffer.
     */
    class Model : public ModelFactory<Model>
    {
    public:
        /**
         * Creates a new empty model.
         */
        Model() = default;

        /**
         * Creates a new model.
         * @tparam T The vertex type. Must be usable to construct a Vertex
         *           (e.g. via std::vector's range constructor), so a plain
         *           std::vector<Vertex> works out of the box.
         * @param vertices The model vertices.
         * @param indices The model indices.
         */
        template <typename T>
        explicit Model(const std::vector<T> &vertices, const std::vector<uint32_t> &indices = {});

        bool CmdRender(const CommandBuffer &commandBuffer, uint32_t instances = 1) const;

        std::vector<Vertex> GetVertices(std::size_t offset = 0) const;
        void SetVertices(std::vector<Vertex> &vertices);

        std::vector<uint32_t> GetIndices(std::size_t offset = 0) const;
        void SetIndices(std::vector<uint32_t> &indices);

        std::vector<float> GetPointCloud() const;

        const Vec3 &GetMinExtents() const { return minExtents; }
        const Vec3 &GetMaxExtents() const { return maxExtents; }

        float GetWidth() const { return maxExtents.x - minExtents.x; }
        float GetHeight() const { return maxExtents.y - minExtents.y; }
        float GetDepth() const { return maxExtents.z - minExtents.z; }

        float GetRadius() const { return radius; }
        const Buffer *GetVertexBuffer() const { return vertexBuffer.get(); }
        const Buffer *GetIndexBuffer() const { return indexBuffer.get(); }

        uint32_t GetVertexCount() const { return vertexCount; }
        uint32_t GetIndexCount() const { return indexCount; }

        static VkIndexType GetIndexType() { return VK_INDEX_TYPE_UINT32; }

    protected:
        // NOTE: intentionally NOT a template. The templated constructor below
        // normalizes any T into std::vector<Vertex> and forwards here, so this
        // single, non-template implementation can live in Model.cpp without
        // causing multiple-definition (ODR) errors across translation units.
        void Initialize(std::vector<Vertex> &vertices, std::vector<uint32_t> &indices);

    private:
        std::unique_ptr<Buffer> vertexBuffer;
        std::unique_ptr<Buffer> indexBuffer;
        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;

        Vec3 minExtents;
        Vec3 maxExtents;
        float radius = 0.0f;
    };

    // Template constructors must be defined where they're visible for
    // instantiation, so this one stays in the header. Everything it calls
    // into is non-template and defined once, in Model.cpp.
    template <typename T>
    Model::Model(const std::vector<T> &vertices, const std::vector<uint32_t> &indices) : Model()
    {
        std::vector<Vertex> vertexData(vertices.begin(), vertices.end());
        std::vector<uint32_t> indexData(indices);
        Initialize(vertexData, indexData);
    }
}