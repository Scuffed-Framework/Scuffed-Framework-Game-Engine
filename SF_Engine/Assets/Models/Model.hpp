#pragma once

#include <cstring>
#include <functional>
#include <unordered_map>

#include <Math/BasicMath.hpp>
#include <Rendering/Mesh/Vertex.hpp>
#include <Rendering/RHI/Memory/Buffer.hpp>

// Cool feature idea:
// import whole .blend files and set up meshes, cameras, and lights.
// to add: usdz and other model types.
namespace SF::Engine
{
    using namespace std;
    template<typename Base>
    class ModelFactory
    {
    public:
        using TCreateReturn = shared_ptr<Base>;

        using TCreateMethodFilename = function<TCreateReturn(const filesystem::path &)>;
        using TRegistryMapFilename  = unordered_map<string, TCreateMethodFilename>;

        virtual ~ModelFactory() = default;

        /**
         * Creates a new model, or finds one with the same values.
         * @param filename The file to load the model from.
         * @return The model loaded from the filename.
         */
        static TCreateReturn Create(const filesystem::path &filename)
        {
            auto fileExt = filename.extension().string();
            auto it      = RegistryFilename().find(fileExt);
            return it == RegistryFilename().end() ? nullptr : it->second(filename);
        }

        static TRegistryMapFilename &RegistryFilename()
        {
            static TRegistryMapFilename impl;
            return impl;
        }

        template<typename T>
        class Registrar : public Base
        {
        protected:
            static bool Register(const string &typeName, const string &extension)
            {
                name                                        = typeName; // per-T, since Registrar<T> has its own static
                ModelFactory::RegistryFilename()[extension] = [](const filesystem::path &filename) -> TCreateReturn
                { return T::Create(filename); };
                return true;
            }

            inline static string name;
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
         *           (e.g. via vector's range constructor), so a plain
         *           vector<Vertex> works out of the box.
         * @param vertices The model vertices.
         * @param indices The model indices.
         */
        template<typename T>
        explicit Model(const vector<T> &vertices, const vector<uint32_t> &indices = {});

        [[nodiscard]] bool CmdRender(const CommandBuffer &commandBuffer, uint32_t instances = 1) const;

        [[nodiscard]] vector<Vertex> GetVertices(size_t offset = 0) const;
        void SetVertices(vector<Vertex> &vertices);

        [[nodiscard]] vector<uint32_t> GetIndices(size_t offset = 0) const;
        void SetIndices(vector<uint32_t> &indices);

        [[nodiscard]] vector<float> GetPointCloud() const;

        [[nodiscard]] const Vec3 &GetMinExtents() const { return minExtents; }
        [[nodiscard]] const Vec3 &GetMaxExtents() const { return maxExtents; }

        [[nodiscard]] float GetWidth() const { return maxExtents.x - minExtents.x; }
        [[nodiscard]] float GetHeight() const { return maxExtents.y - minExtents.y; }
        [[nodiscard]] float GetDepth() const { return maxExtents.z - minExtents.z; }

        [[nodiscard]] float GetRadius() const { return radius; }
        [[nodiscard]] const Buffer *GetVertexBuffer() const { return vertexBuffer.get(); }
        [[nodiscard]] const Buffer *GetIndexBuffer() const { return indexBuffer.get(); }

        [[nodiscard]] uint32_t GetVertexCount() const { return vertexCount; }
        [[nodiscard]] uint32_t GetIndexCount() const { return indexCount; }

        static VkIndexType GetIndexType() { return VK_INDEX_TYPE_UINT32; }

    protected:
        // NOTE: intentionally NOT a template. The templated constructor below
        // normalizes any T into vector<Vertex> and forwards here, so this
        // single, non-template implementation can live in Model.cpp without
        // causing multiple-definition (ODR) errors across translation units.
        void Initialize(vector<Vertex> &vertices, vector<uint32_t> &indices);

    private:
        unique_ptr<Buffer> vertexBuffer;
        unique_ptr<Buffer> indexBuffer;
        uint32_t vertexCount = 0;
        uint32_t indexCount  = 0;

        Vec3 minExtents = {};
        Vec3 maxExtents = {};
        float radius    = 0.0f;
    };

    // Template constructors must be defined where they're visible for
    // instantiation, so this one stays in the header. Everything it calls
    // into is non-template and defined once, in Model.cpp.
    template<typename T>
    Model::Model(const vector<T> &vertices, const vector<uint32_t> &indices) : Model()
    {
        vector<Vertex> vertexData(vertices.begin(), vertices.end());
        vector<uint32_t> indexData(indices);
        Initialize(vertexData, indexData);
    }
} // namespace SF::Engine
