#include "GltfModel.hpp"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "TinyGltf.hpp"

#include <Filesystem/File.hpp>
#include <iostream>
#include <Rendering/Mesh/Mesh.hpp>

namespace SF::Engine
{
    namespace
    {
        template <typename T>
        const T *GetAccessorData(const tinygltf::Model &model, const tinygltf::Accessor &accessor)
        {
            const auto &bufferView = model.bufferViews[accessor.bufferView];
            const auto &buffer = model.buffers[bufferView.buffer];
            auto offset = bufferView.byteOffset + accessor.byteOffset;
            return reinterpret_cast<const T *>(&buffer.data[offset]);
        }
    }

    std::shared_ptr<GltfModel> GltfModel::Create(const std::filesystem::path &filename)
    {
        return std::make_shared<GltfModel>(filename);
    }

    GltfModel::GltfModel(std::filesystem::path filename, bool load) : filename(std::move(filename))
    {
        if (load)
        {
            Load();
        }
    }

    void GltfModel::Load()
    {
        if (filename.empty())
        {
            return;
        }

        if (!File::Exists(filename))
        {
            throw std::runtime_error("GLTF file does not exist: " + filename.string());
        }

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string warn, err;

        bool ok = false;
        auto ext = filename.extension().string();
        if (ext == ".glb")
        {
            ok = loader.LoadBinaryFromFile(&model, &err, &warn, filename.string());
        }
        else
        {
            ok = loader.LoadASCIIFromFile(&model, &err, &warn, filename.string());
        }

        if (!warn.empty())
        {
            std::cerr << "GLTF warning: " << warn << "\n";
        }
        if (!ok)
        {
            throw std::runtime_error("Failed to load GLTF file: " + filename.string() + " - " + err);
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (const auto &mesh : model.meshes)
        {
            for (const auto &primitive : mesh.primitives)
            {
                if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
                {
                    continue; // todo: handle fans/strips
                }

                auto posIt = primitive.attributes.find("POSITION");
                if (posIt == primitive.attributes.end())
                {
                    continue;
                }

                const auto &posAccessor = model.accessors[posIt->second];
                const auto *positions = GetAccessorData<float>(model, posAccessor);
                auto vertexCount = posAccessor.count;

                const float *normals = nullptr;
                if (auto it = primitive.attributes.find("NORMAL"); it != primitive.attributes.end())
                {
                    normals = GetAccessorData<float>(model, model.accessors[it->second]);
                }

                const float *uvs = nullptr;
                if (auto it = primitive.attributes.find("TEXCOORD_0"); it != primitive.attributes.end())
                {
                    uvs = GetAccessorData<float>(model, model.accessors[it->second]);
                }

                auto vertexOffset = static_cast<uint32_t>(vertices.size());

                for (size_t i = 0; i < vertexCount; i++)
                {
                    Vec3 position(positions[3 * i], positions[3 * i + 1], positions[3 * i + 2]);

                    Vec3 normal;
                    if (normals)
                    {
                        normal = Vec3(normals[3 * i], normals[3 * i + 1], normals[3 * i + 2]);
                    }

                    Vec2 uv;
                    if (uvs)
                    {
                        uv = Vec2(uvs[2 * i], uvs[2 * i + 1]);
                    }

                    vertices.emplace_back(position, uv, normal);
                }

                if (primitive.indices >= 0)
                {
                    const auto &indexAccessor = model.accessors[primitive.indices];

                    switch (indexAccessor.componentType)
                    {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    {
                        const auto *idx = GetAccessorData<uint16_t>(model, indexAccessor);
                        for (size_t i = 0; i < indexAccessor.count; i++)
                        {
                            indices.emplace_back(vertexOffset + idx[i]);
                        }
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    {
                        const auto *idx = GetAccessorData<uint32_t>(model, indexAccessor);
                        for (size_t i = 0; i < indexAccessor.count; i++)
                        {
                            indices.emplace_back(vertexOffset + idx[i]);
                        }
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    {
                        const auto *idx = GetAccessorData<uint8_t>(model, indexAccessor);
                        for (size_t i = 0; i < indexAccessor.count; i++)
                        {
                            indices.emplace_back(vertexOffset + idx[i]);
                        }
                        break;
                    }
                    default:
                        throw std::runtime_error("Unsupported GLTF index component type");
                    }
                }
                else
                {
                    // No index buffer: positions are already in draw order.
                    for (size_t i = 0; i < vertexCount; i++)
                    {
                        indices.emplace_back(vertexOffset + static_cast<uint32_t>(i));
                    }
                }
            }
        }

        Initialize(vertices, indices);
    }
}