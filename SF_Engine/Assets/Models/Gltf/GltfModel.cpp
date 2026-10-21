#include "GltfModel.hpp"

#include <Engine/Log/Log.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <Rendering/Mesh/Mesh.hpp>

// FUCK YOU X.h!
#ifdef None
    #undef None
#endif

#include <fastgltf/core.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace SF::Engine
{
    namespace
    {
        template<typename It>
        std::size_t AttrAccessorIndex(const It &it)
        {
            if constexpr (requires { it->accessorIndex; })
                return it->accessorIndex;
            else
                return it->second;
        }

        struct F3
        {
            float x, y, z;
        };

        // fastgltf matrices are column-major: m[column][row].
        F3 TransformPoint(const fastgltf::math::fmat4x4 &m, float x, float y, float z)
        {
            return {m[0][0] * x + m[1][0] * y + m[2][0] * z + m[3][0],
                    m[0][1] * x + m[1][1] * y + m[2][1] * z + m[3][1],
                    m[0][2] * x + m[1][2] * y + m[2][2] * z + m[3][2]};
        }

        // Upper 3x3 only, renormalized. Exact for rotation + uniform scale; non-uniform scale
        // would need the inverse-transpose.
        F3 TransformDirection(const fastgltf::math::fmat4x4 &m, float x, float y, float z)
        {
            F3 d{m[0][0] * x + m[1][0] * y + m[2][0] * z, m[0][1] * x + m[1][1] * y + m[2][1] * z,
                 m[0][2] * x + m[1][2] * y + m[2][2] * z};
            const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
            if (len > 0.0f)
            {
                d.x /= len;
                d.y /= len;
                d.z /= len;
            }
            return d;
        }

        // Area-weighted smooth normals for primitives that don't ship NORMAL.
        void GenerateNormals(std::vector<Vertex> &vertices, uint32_t base, size_t vertexCount,
                             const std::vector<uint32_t> &indices, size_t indexStart)
        {
            std::vector<F3> sum(vertexCount, F3{0.0f, 0.0f, 0.0f});

            for (size_t i = indexStart; i + 2 < indices.size(); i += 3)
            {
                const uint32_t ia = indices[i], ib = indices[i + 1], ic = indices[i + 2];
                const auto &a = vertices[ia].position;
                const auto &b = vertices[ib].position;
                const auto &c = vertices[ic].position;

                const float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
                const float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
                const F3 n{e1y * e2z - e1z * e2y, e1z * e2x - e1x * e2z, e1x * e2y - e1y * e2x};

                for (const uint32_t idx: {ia, ib, ic})
                {
                    sum[idx - base].x += n.x;
                    sum[idx - base].y += n.y;
                    sum[idx - base].z += n.z;
                }
            }

            for (size_t i = 0; i < vertexCount; ++i)
            {
                F3 n            = sum[i];
                const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
                if (len > 0.0f)
                {
                    n.x /= len;
                    n.y /= len;
                    n.z /= len;
                } else
                    n = {0.0f, 1.0f, 0.0f};
                vertices[base + i].normal = Vec3(n.x, n.y, n.z);
            }
        }
    } // namespace

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

        // FromPath reads the whole file; loadGltf detects .gltf (JSON) vs .glb (binary) itself.
        auto data = fastgltf::GltfDataBuffer::FromPath(filename);
        if (data.error() != fastgltf::Error::None)
        {
            throw std::runtime_error("GLTF: failed to read '" + filename.string() +
                                     "': " + std::string(fastgltf::getErrorMessage(data.error())));
        }

        // External .bin files are resolved relative to the .gltf's directory.
        fastgltf::Parser parser;
        auto asset = parser.loadGltf(data.get(), filename.parent_path(), fastgltf::Options::LoadExternalBuffers);
        if (asset.error() != fastgltf::Error::None)
        {
            throw std::runtime_error("GLTF: failed to parse '" + filename.string() +
                                     "': " + std::string(fastgltf::getErrorMessage(asset.error())));
        }
        auto &gltf = asset.get();

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        // Appends every triangle primitive of `mesh`, baked into model space with `world`.
        const auto appendMesh = [&](const fastgltf::Mesh &mesh, const fastgltf::math::fmat4x4 &world)
        {
            for (const auto &primitive: mesh.primitives)
            {
                if (primitive.type != fastgltf::PrimitiveType::Triangles)
                {
                    Log::Warning(
                            "GltfModel '{}': skipped a non-triangle primitive (only triangle lists are supported).",
                            filename.string());
                    continue;
                }

                const auto positionAttr = primitive.findAttribute("POSITION");
                if (positionAttr == primitive.attributes.end())
                {
                    Log::Warning("GltfModel '{}': skipped a primitive with no POSITION attribute.", filename.string());
                    continue;
                }

                const auto &positions = gltf.accessors[AttrAccessorIndex(positionAttr)];
                if (!positions.bufferViewIndex.has_value())
                    continue; // nothing to read

                const auto base        = static_cast<uint32_t>(vertices.size());
                const size_t indexBase = indices.size();
                vertices.resize(vertices.size() + positions.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        gltf, positions,
                        [&](const fastgltf::math::fvec3 &p, std::size_t i)
                        {
                            const F3 w                  = TransformPoint(world, p[0], p[1], p[2]);
                            vertices[base + i].position = Vec3(w.x, w.y, w.z);
                        });

                bool hasNormals = false;
                if (const auto it = primitive.findAttribute("NORMAL");
                    it != primitive.attributes.end() &&
                    gltf.accessors[AttrAccessorIndex(it)].bufferViewIndex.has_value())
                {
                    hasNormals = true;
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                            gltf, gltf.accessors[AttrAccessorIndex(it)],
                            [&](const fastgltf::math::fvec3 &n, std::size_t i)
                            {
                                if (base + i >= vertices.size())
                                    return;
                                const F3 w                = TransformDirection(world, n[0], n[1], n[2]);
                                vertices[base + i].normal = Vec3(w.x, w.y, w.z);
                            });
                }

                // glTF UV origin is top-left, same as Vulkan, so no V flip here.
                if (const auto it = primitive.findAttribute("TEXCOORD_0");
                    it != primitive.attributes.end() &&
                    gltf.accessors[AttrAccessorIndex(it)].bufferViewIndex.has_value())
                {
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                            gltf, gltf.accessors[AttrAccessorIndex(it)],
                            [&](const fastgltf::math::fvec2 &uv, std::size_t i)
                            {
                                if (base + i >= vertices.size())
                                    return;
                                vertices[base + i].texCoord = Vec2(uv[0], uv[1]);
                            });
                }

                bool badIndex = false;
                if (primitive.indicesAccessor.has_value())
                {
                    const auto &indexAccessor = gltf.accessors[*primitive.indicesAccessor];
                    indices.reserve(indices.size() + indexAccessor.count);
                    fastgltf::iterateAccessor<std::uint32_t>(gltf, indexAccessor,
                                                             [&](std::uint32_t index)
                                                             {
                                                                 if (index >= positions.count)
                                                                 {
                                                                     badIndex = true;
                                                                     return;
                                                                 }
                                                                 indices.push_back(base + index);
                                                             });
                } else
                {
                    // Non-indexed primitive: vertices are already in triangle order.
                    for (size_t i = 0; i < positions.count; ++i)
                        indices.push_back(base + static_cast<uint32_t>(i));
                }

                if (badIndex)
                {
                    Log::Warning("GltfModel '{}': dropped a primitive with out-of-range indices.", filename.string());
                    vertices.resize(base);
                    indices.resize(indexBase);
                    continue;
                }

                if (!hasNormals)
                    GenerateNormals(vertices, base, positions.count, indices, indexBase);
            }
        };

        if (gltf.scenes.empty())
        {
            // No scene graph: take every mesh as-is.
            for (const auto &mesh: gltf.meshes)
                appendMesh(mesh, fastgltf::math::fmat4x4());
        } else
        {
            std::size_t sceneIndex = gltf.defaultScene.value_or(0);
            if (sceneIndex >= gltf.scenes.size())
                sceneIndex = 0;

            // Walks the node hierarchy, handing us each node's accumulated world matrix.
            fastgltf::iterateSceneNodes(gltf, sceneIndex, fastgltf::math::fmat4x4(),
                                        [&](fastgltf::Node &node, const fastgltf::math::fmat4x4 &world)
                                        {
                                            if (node.meshIndex.has_value())
                                                appendMesh(gltf.meshes[*node.meshIndex], world);
                                        });
        }

        if (vertices.empty() || indices.empty())
        {
            throw std::runtime_error("GLTF contains no triangle geometry: " + filename.string());
        }

        Initialize(vertices, indices);
    }
} // namespace SF::Engine
