#include "FbxModel.hpp"

#include <fstream>
#include <ofbx.h>

#include <Filesystem/File.hpp>
#include <iostream>
#include <Rendering/Mesh/Mesh.hpp>

namespace SF::Engine
{
    namespace
    {
        // OpenFBX encodes the last index of each polygon as the bitwise
        // complement of the real index, so consumers can find polygon
        // boundaries without a separate per-face count array.
        uint32_t UnpackFbxIndex(int rawIndex)
        {
            return static_cast<uint32_t>(rawIndex < 0 ? -rawIndex - 1 : rawIndex);
        }
    }

    std::shared_ptr<FbxModel> FbxModel::Create(const std::filesystem::path &filename)
    {
        return std::make_shared<FbxModel>(filename);
    }

    FbxModel::FbxModel(std::filesystem::path filename, bool load) : filename(std::move(filename))
    {
        if (load)
        {
            Load();
        }
    }

    void FbxModel::Load()
    {
        if (filename.empty())
        {
            return;
        }

        if (!File::Exists(filename))
        {
            throw std::runtime_error("FBX file does not exist: " + filename.string());
        }

        std::ifstream inStream(filename, std::ios::binary | std::ios::ate);
        if (!inStream)
        {
            throw std::runtime_error("Failed to open FBX file: " + filename.string());
        }

        auto fileSize = static_cast<std::size_t>(inStream.tellg());
        inStream.seekg(0);

        std::vector<ofbx::u8> fileData(fileSize);
        if (!inStream.read(reinterpret_cast<char *>(fileData.data()), static_cast<std::streamsize>(fileSize)))
        {
            throw std::runtime_error("Failed to read FBX file: " + filename.string());
        }

        ofbx::IScene *scene = ofbx::load(fileData.data(), static_cast<int>(fileData.size()),
                                          static_cast<ofbx::u64>(ofbx::LoadFlags::TRIANGULATE));
        if (!scene)
        {
            throw std::runtime_error(std::string("Failed to parse FBX file: ") + ofbx::getError());
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        auto meshCount = scene->getMeshCount();
        for (int meshIndex = 0; meshIndex < meshCount; meshIndex++)
        {
            const ofbx::Mesh *mesh = scene->getMesh(meshIndex);
            const ofbx::Geometry *geom = mesh->getGeometry();
            if (!geom)
            {
                continue;
            }

            const ofbx::Vec3 *positions = geom->getVertices();
            const ofbx::Vec3 *normals = geom->getNormals();
            const ofbx::Vec2 *uvs = geom->getUVs();
            const int *rawIndices = geom->getFaceIndices();

            auto vertexCount = geom->getVertexCount();
            auto indexCount = geom->getIndexCount();

            auto vertexOffset = static_cast<uint32_t>(vertices.size());

            for (int i = 0; i < vertexCount; i++)
            {
                ofbx::Vec3 position = positions[i];
                Vec3 pos(static_cast<float>(position.x), static_cast<float>(position.y), static_cast<float>(position.z));

                Vec3 normal;
                if (normals)
                {
                    ofbx::Vec3 n = normals[i];
                    normal = Vec3(static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z));
                }

                Vec3 uv;
                if (uvs)
                {
                    ofbx::Vec2 uvValue = uvs[i];
                    uv = Vec3(static_cast<float>(uvValue.x), 1.0f - static_cast<float>(uvValue.y), 0);
                }

                vertices.emplace_back(pos, uv, normal);
            }

            // getVertices()/getNormals()/getUVs() are already expanded per-face
            // (one entry per index, not deduplicated per-vertex), so the raw
            // face index array maps 1:1 onto the arrays above.
            for (int i = 0; i < indexCount; i++)
            {
                indices.emplace_back(vertexOffset + UnpackFbxIndex(rawIndices[i]));
            }
        }

        scene->destroy();

        Initialize(vertices, indices);
    }
}