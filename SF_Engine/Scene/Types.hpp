#pragma once
#include <EntityComponentSystem/Entity.hpp>
#include <Math/Transform.hpp>
#include <Rendering/Lighting/Light.hpp>
#include <Rendering/Lighting/LightingTypes.hpp>
#include <Rendering/Lighting/LitMeshSFERenderpass.hpp>
#include <Rendering/Mesh/Mesh.hpp>
#include <memory>
#include <string>

namespace SF::Engine
{
    struct SceneObject : public Entity, public Serializable
    {
    public:
        std::shared_ptr<Mesh> mesh;
        bool enabled = true;
        std::string meshSourcePath; // e.g. "assets/meshes/cube.obj"

        // Asset references. Scenes store the stable asset UUID, never a path; Scene::ResolveBuiltinMesh
        // turns these into live resources once the renderer exists.
        UUID meshAsset;      // MeshAsset providing `mesh` (null = builtin / none)
        int meshPart = 0;    // index into MeshAsset::parts
        UUID albedoAsset;    // ImageAsset<Image2d> used as MeshMaterial::albedo (null = none)
        UUID audioAsset;     // AudioAsset played by this object's AudioClip (null = not an emitter)
        bool audioLoop    = false;
        float audioGain   = 1.0f;
        float audioPitch  = 1.0f;

        SceneObject(const std::string &objName) : Entity(objName) { AddComponent<MeshMaterial>(); }

        void Serialize(XMLNode &node) const override
        {
            node.SetAttribute("name", GetName());
            node.SetAttribute("enabled", enabled);
            node.SetAttribute("mesh", meshSourcePath);

            if (!meshAsset.IsNull())
            {
                node.SetAttribute("meshAsset", meshAsset);
                node.SetAttribute("meshPart", meshPart);
            }
            if (!albedoAsset.IsNull())
                node.SetAttribute("albedoAsset", albedoAsset);
            if (!audioAsset.IsNull())
            {
                node.SetAttribute("audioAsset", audioAsset);
                node.SetAttribute("audioLoop", audioLoop);
                node.SetAttribute("audioGain", audioGain);
                node.SetAttribute("audioPitch", audioPitch);
            }

            XMLNode tNode = node.AddChild("transform");
            Entity::GetComponent<Transform>()->Serialize(tNode);

            XMLNode matNode = node.AddChild("material");
            Entity::GetComponent<MeshMaterial>()->Serialize(matNode);
        }

        void Deserialize(const XMLNode &node) override
        {
            std::string n;
            node.GetAttribute("name", n);
            SetName(n);
            node.GetAttribute("enabled", enabled);
            node.GetAttribute("mesh", meshSourcePath); // caller loads the asset
            node.GetAttribute("meshAsset", meshAsset);
            node.GetAttribute("meshPart", meshPart);
            node.GetAttribute("albedoAsset", albedoAsset);
            node.GetAttribute("audioAsset", audioAsset);
            node.GetAttribute("audioLoop", audioLoop);
            node.GetAttribute("audioGain", audioGain);
            node.GetAttribute("audioPitch", audioPitch);

            XMLNode tNode = node.GetChild("transform");
            if (tNode.IsValid())
                Entity::GetComponent<Transform>()->Deserialize(tNode);

            XMLNode matNode = node.GetChild("material");
            if (matNode.IsValid())
                Entity::GetComponent<MeshMaterial>()->Deserialize(matNode);
        }
    };

    struct SceneLight : public Entity, public Serializable
    {
        SceneLight(const std::string &lightName) : Entity(lightName) { AddComponent<Light>(); }

        void Serialize(XMLNode &node) const override
        {
            node.SetAttribute("name", GetName());

            XMLNode tNode = node.AddChild("transform");
            Entity::GetComponent<Transform>()->Serialize(tNode);

            XMLNode lNode = node.AddChild("light");
            Entity::GetComponent<Light>()->Serialize(lNode);
        }

        void Deserialize(const XMLNode &node) override
        {
            std::string n;
            node.GetAttribute("name", n);
            SetName(n);

            XMLNode tNode = node.GetChild("transform");
            if (tNode.IsValid())
                Entity::GetComponent<Transform>()->Deserialize(tNode);

            XMLNode lNode = node.GetChild("light");
            if (lNode.IsValid())
                Entity::GetComponent<Light>()->Deserialize(lNode);
        }
    };
} // namespace SF::Engine
