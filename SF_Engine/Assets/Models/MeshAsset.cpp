#include "MeshAsset.hpp"

#include <Engine/Log/Log.hpp>

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cfloat>
#include <filesystem>

namespace SF::Engine
{
    namespace
    {
        // assimp matrices are row-major with translation in the 4th column; glm is column-major.
        Mat4 ToGlm(const aiMatrix4x4 &m)
        {
            return Mat4(m.a1, m.b1, m.c1, m.d1, //
                        m.a2, m.b2, m.c2, m.d2, //
                        m.a3, m.b3, m.c3, m.d3, //
                        m.a4, m.b4, m.c4, m.d4);
        }

        Vec3 SafeNormalize(const Vec3 &v, const Vec3 &fallback)
        {
            const float len = glm::length(v);
            return len > 1e-8f ? v / len : fallback;
        }
    } // namespace

    bool MeshAsset::EnsureLoaded()
    {
        if (!parts.empty())
            return true;

        loadError.clear();
        if (assetPath.empty() || !std::filesystem::exists(assetPath))
        {
            loadError = "source file is missing: " + assetPath.string();
            Log::Error("MeshAsset '{}': {}", name, loadError);
            return false;
        }

        Assimp::Importer importer;
        importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

        // aiProcess_PreTransformVertices is deliberately NOT used: we bake node transforms ourselves so each
        // material stays its own part.
        unsigned flags = aiProcess_Triangulate | aiProcess_SortByPType | aiProcess_GenUVCoords |
                         aiProcess_ValidateDataStructure;
        if (settings.joinIdenticalVertices)
            flags |= aiProcess_JoinIdenticalVertices;
        if (settings.generateNormals)
            flags |= aiProcess_GenSmoothNormals;
        if (settings.generateTangents)
            flags |= aiProcess_CalcTangentSpace;
        if (settings.optimizeCache)
            flags |= aiProcess_ImproveCacheLocality;
        if (settings.flipUVs)
            flags |= aiProcess_FlipUVs;

        // NOTE: ReadFile takes a narrow path; non-ASCII paths on Windows may fail inside assimp.
        const aiScene *scene = importer.ReadFile(assetPath.string().c_str(), flags);
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
        {
            loadError = importer.GetErrorString();
            if (loadError.empty())
                loadError = "assimp produced an incomplete scene";
            Log::Error("MeshAsset '{}': {}", name, loadError);
            return false;
        }

        std::vector<Part> built;
        Vec3 bmin(FLT_MAX);
        Vec3 bmax(-FLT_MAX);
        uint32_t skinned = 0;
        uint32_t skipped = 0;

        struct Frame
        {
            const aiNode *node;
            Mat4 parentWorld;
        };
        std::vector<Frame> stack;
        stack.push_back({scene->mRootNode, Mat4(1.0f)});

        while (!stack.empty())
        {
            const Frame frame = stack.back();
            stack.pop_back();
            const Mat4 world = frame.parentWorld * ToGlm(frame.node->mTransformation);

            const glm::mat3 linear = glm::mat3(world);
            const glm::mat3 normalMat = glm::transpose(glm::inverse(linear));
            const bool mirrored       = glm::determinant(linear) < 0.0f; // flips triangle winding

            for (unsigned mi = 0; mi < frame.node->mNumMeshes; ++mi)
            {
                const aiMesh *m = scene->mMeshes[frame.node->mMeshes[mi]];
                if (m->mPrimitiveTypes != aiPrimitiveType_TRIANGLE || m->mNumVertices == 0 || m->mNumFaces == 0)
                {
                    ++skipped;
                    continue;
                }
                if (m->HasBones())
                    ++skinned;

                std::vector<Vertex> verts(m->mNumVertices);
                for (unsigned v = 0; v < m->mNumVertices; ++v)
                {
                    const aiVector3D &p = m->mVertices[v];
                    Vertex &out         = verts[v];

                    out.position = Vec3(world * Vec4(p.x, p.y, p.z, 1.0f)) * settings.scale;

                    if (m->HasNormals())
                    {
                        const aiVector3D &n = m->mNormals[v];
                        out.normal          = SafeNormalize(normalMat * Vec3(n.x, n.y, n.z), Vec3(0, 1, 0));
                    } else
                        out.normal = Vec3(0, 1, 0);

                    if (m->HasTextureCoords(0))
                        out.texCoord = Vec2(m->mTextureCoords[0][v].x, m->mTextureCoords[0][v].y);

                    if (m->HasTangentsAndBitangents())
                    {
                        const aiVector3D &t = m->mTangents[v];
                        out.tangent         = SafeNormalize(linear * Vec3(t.x, t.y, t.z), Vec3(1, 0, 0));
                    } else
                        out.tangent = Vec3(1, 0, 0);

                    bmin = glm::min(bmin, out.position);
                    bmax = glm::max(bmax, out.position);
                }

                std::vector<uint32_t> indices;
                indices.reserve(static_cast<size_t>(m->mNumFaces) * 3);
                for (unsigned f = 0; f < m->mNumFaces; ++f)
                {
                    const aiFace &face = m->mFaces[f];
                    if (face.mNumIndices != 3)
                        continue;
                    indices.push_back(face.mIndices[0]);
                    indices.push_back(face.mIndices[mirrored ? 2 : 1]);
                    indices.push_back(face.mIndices[mirrored ? 1 : 2]);
                }
                if (indices.empty())
                {
                    ++skipped;
                    continue;
                }

                Part part;
                part.name = m->mName.length > 0 ? std::string(m->mName.C_Str()) : std::string(frame.node->mName.C_Str());
                if (part.name.empty())
                    part.name = name + "_" + std::to_string(built.size());
                part.vertexCount   = m->mNumVertices;
                part.triangleCount = static_cast<uint32_t>(indices.size() / 3);

                if (m->mMaterialIndex < scene->mNumMaterials)
                {
                    const aiMaterial *mat = scene->mMaterials[m->mMaterialIndex];
                    aiColor4D color;
                    if (mat->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS ||
                        mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
                        part.baseColor = Vec4(color.r, color.g, color.b, color.a);

                    float factor = 0.0f;
                    if (mat->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS)
                        part.metallic = factor;
                    if (mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS)
                        part.roughness = factor;
                }

                try
                {
                    part.mesh = std::make_shared<Mesh>(verts, indices);
                } catch (const std::exception &e)
                {
                    Log::Error("MeshAsset '{}': GPU upload of '{}' failed: {}", name, part.name, e.what());
                    continue;
                }
                built.push_back(std::move(part));
            }

            for (unsigned c = frame.node->mNumChildren; c-- > 0;)
                stack.push_back({frame.node->mChildren[c], world});
        }

        if (built.empty())
        {
            loadError = "the file contains no triangle geometry";
            Log::Error("MeshAsset '{}': {}", name, loadError);
            return false;
        }

        animationCount   = scene->mNumAnimations;
        skinnedMeshCount = skinned;
        if (animationCount > 0 || skinnedMeshCount > 0)
            Log::Warning("MeshAsset '{}': {} animation(s) / {} skinned mesh(es) ignored (not implemented yet).", name,
                         animationCount, skinnedMeshCount);
        if (skipped > 0)
            Log::Warning("MeshAsset '{}': skipped {} non-triangle or empty mesh(es).", name, skipped);

        parts     = std::move(built);
        boundsMin = bmin;
        boundsMax = bmax;
        return true;
    }

    void MeshAsset::SerializeImportSettings(XMLNode &node) const
    {
        node.SetAttribute("scale", settings.scale);
        node.SetAttribute("flipUVs", settings.flipUVs);
        node.SetAttribute("generateNormals", settings.generateNormals);
        node.SetAttribute("generateTangents", settings.generateTangents);
        node.SetAttribute("joinIdenticalVertices", settings.joinIdenticalVertices);
        node.SetAttribute("optimizeCache", settings.optimizeCache);
    }

    void MeshAsset::DeserializeImportSettings(const XMLNode &node)
    {
        node.GetAttribute("scale", settings.scale);
        node.GetAttribute("flipUVs", settings.flipUVs);
        node.GetAttribute("generateNormals", settings.generateNormals);
        node.GetAttribute("generateTangents", settings.generateTangents);
        node.GetAttribute("joinIdenticalVertices", settings.joinIdenticalVertices);
        node.GetAttribute("optimizeCache", settings.optimizeCache);
    }
} // namespace SF::Engine
