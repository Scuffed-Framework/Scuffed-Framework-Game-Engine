#pragma once

#include <Assets/AssetPipeline.hpp>
#include <Math/BasicMath.hpp>
#include <Rendering/Mesh/Mesh.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace SF::Engine
{
    struct ModelImportSettings
    {
        float scale                = 1.0f; // uniform, applied to positions at load (FBX is often in cm)
        bool flipUVs               = true; // assimp UVs are bottom-left origin; Vulkan samples from top-left
        bool generateNormals       = true; // smooth normals when the file has none
        bool generateTangents      = true;
        bool joinIdenticalVertices = true;
        bool optimizeCache         = true;
    };

    // A model file imported through assimp. Static geometry only: skins and animations are ignored.
    // Node transforms are baked into the vertices, and the model is split into one Mesh per assimp mesh
    // (i.e. per material), because SceneObject renders one Mesh with one MeshMaterial.
    class MeshAsset : public AssetBase
    {
        SF_RTTI(MeshAsset, AssetBase)
    public:
        struct Part
        {
            std::string name;
            std::shared_ptr<Mesh> mesh;
            Vec4 baseColor  = {1.0f, 1.0f, 1.0f, 1.0f};
            float metallic  = 0.0f;
            float roughness = 1.0f;
            uint32_t vertexCount   = 0;
            uint32_t triangleCount = 0;
        };

        MeshAsset() { type = AssetType::Mesh; }

        ModelImportSettings settings;

        // Valid after EnsureLoaded().
        std::vector<Part> parts;
        Vec3 boundsMin{0.0f};
        Vec3 boundsMax{0.0f};
        uint32_t animationCount   = 0; // present in the file, ignored
        uint32_t skinnedMeshCount = 0; // present in the file, ignored
        std::string loadError;

        [[nodiscard]] bool IsLoaded() const { return !parts.empty(); }
        void Unload() { parts.clear(); }

        bool EnsureLoaded() override;
        void Save() override { SaveMeta(); }
        bool Load(span<const uint8_t> /*payload*/) override { return EnsureLoaded(); }

        void SerializeImportSettings(XMLNode &node) const override;
        void DeserializeImportSettings(const XMLNode &node) override;
    };
} // namespace SF::Engine
