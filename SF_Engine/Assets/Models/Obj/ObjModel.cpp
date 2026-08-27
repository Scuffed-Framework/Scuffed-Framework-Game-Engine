#include "ObjModel.hpp"

#include <tiny_obj_loader.h>

#include <LowLevel/FileSystem/File.hpp>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <Rendering/Mesh/Mesh.hpp>

namespace
{
    // Assumed Vertex layout: position (Vec3), uv (Vec2), normal (Vec3).
    // Adjust field names here if Vertex.hpp differs.
    bool operator==(const SF::Engine::Vertex &a, const SF::Engine::Vertex &b)
    {
        return a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z &&
               a.texCoord.x == b.texCoord.x && a.texCoord.y == b.texCoord.y &&
               a.normal.x == b.normal.x && a.normal.y == b.normal.y && a.normal.z == b.normal.z;
    }
}

namespace std
{
    template <>
    struct hash<SF::Engine::Vertex>
    {
        size_t operator()(const SF::Engine::Vertex &v) const noexcept
        {
            size_t h = std::hash<float>()(v.position.x);
            h ^= std::hash<float>()(v.position.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<float>()(v.position.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<float>()(v.texCoord.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<float>()(v.texCoord.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<float>()(v.normal.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<float>()(v.normal.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<float>()(v.normal.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}

namespace SF::Engine
{
    class MaterialStreamReader : public tinyobj::MaterialReader
    {
    public:
        explicit MaterialStreamReader(std::filesystem::path folder) : folder(std::move(folder))
        {
        }

        bool operator()(const std::string &matId, std::vector<tinyobj::material_t> *materials, std::map<std::string, int> *matMap, std::string *warn, std::string *err) override
        {
            auto filepath = folder / matId;

            if (!File::Exists(filepath))
            {
                std::stringstream ss;
                ss << "Material stream in error state. \n";

                if (warn)
                {
                    (*warn) += ss.str();
                }

                return false;
            }

            std::ifstream inStream(filepath);
            tinyobj::LoadMtl(matMap, materials, &inStream, warn, err);
            return true;
        }

    private:
        std::filesystem::path folder;
    };

    std::shared_ptr<ObjModel> ObjModel::Create(const std::filesystem::path &filename)
    {
        return std::make_shared<ObjModel>(filename);
    }

    ObjModel::ObjModel(std::filesystem::path filename, bool load) : filename(std::move(filename))
    {
        if (load)
        {
            Load();
        }
    }

    void ObjModel::Load()
    {
        if (filename.empty())
        {
            return;
        }

        auto folder = filename.parent_path();
        std::ifstream inStream(filename);
        MaterialStreamReader materialReader(folder);

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, &inStream, &materialReader))
        {
            throw std::runtime_error(warn + err);
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<Vertex, uint32_t> uniqueVertices;

        for (const auto &shape : shapes)
        {
            for (const auto &index : shape.mesh.indices)
            {
                Vec3 position(
                    attrib.vertices[3 * index.vertex_index],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]);

                Vec3 uv;
                if (index.texcoord_index >= 0 && !attrib.texcoords.empty())
                {
                    uv = Vec3(
                        attrib.texcoords[2 * index.texcoord_index],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
                        attrib.texcoords[2*index.texcoord_index + 2]
                    );
                }

                Vec3 normal;
                if (index.normal_index >= 0 && !attrib.normals.empty())
                {
                    normal = Vec3(
                        attrib.normals[3 * index.normal_index],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]);
                }

                Vertex vertex(position, uv, normal);

                auto it = uniqueVertices.find(vertex);
                if (it == uniqueVertices.end())
                {
                    auto newIndex = static_cast<uint32_t>(vertices.size());
                    uniqueVertices.emplace(vertex, newIndex);
                    vertices.emplace_back(vertex);
                    indices.emplace_back(newIndex);
                }
                else
                {
                    indices.emplace_back(it->second);
                }
            }
        }

        Initialize(vertices, indices);
    }
}