#pragma once
#include <string>
#include <memory>
#include <Rendering/Mesh/Mesh.hpp>
#include <Rendering/Lighting/LightingTypes.hpp>
#include <Math/Transform.hpp>
#include <Rendering/Lighting/Light.hpp>
#include <Rendering/Lighting/LitMeshPipelinePass.hpp>
#include <Entity/Entity.hpp>

namespace SF::Engine
{
    struct SceneObject : public Entity, public Serializable
    {
    public:
        std::shared_ptr<Mesh> mesh;
        bool enabled = true;
        std::string meshSourcePath; // e.g. "assets/meshes/cube.obj"

        SceneObject(const std::string &objName, Entity *objParent = nullptr)
            : Entity(objName, objParent)
        {
            Entity::AddComponent<MeshMaterial>();
        }

        void Serialize(XMLNode &node) const override
        {
            node.SetAttribute("name", GetName());
            node.SetAttribute("enabled", enabled);
            node.SetAttribute("mesh", meshSourcePath);

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
        SceneLight(const std::string &lightName, Entity *lightParent = nullptr)
            : Entity(lightName, lightParent)
        {
            Entity::AddComponent<Light>();
        }

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
}