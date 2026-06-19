#pragma once
#include <string>
#include <memory>
#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Lighting/LightingTypes.hpp>
#include <Components/TransformComponent.hpp>
#include <XML/XMLReader.hpp>
#include <Graphics/Lighting/Light.hpp>

namespace SF::Engine
{
    struct SceneObject : public Serializable
    {
        std::string name;
        TransformComponent transform;
        MeshMaterial material;
        std::shared_ptr<Mesh> mesh;
        bool enabled = true;

        void Serialize(XMLNode &node) const override
        {
            node.SetAttribute("name", name);
            node.SetAttribute("enabled", enabled);
            // Store a path or primitive shortcut so Deserialize can reconstruct the mesh
            node.SetAttribute("mesh", meshSourcePath);

            XMLNode tNode = node.AddChild("transform");
            transform.Serialize(tNode);

            XMLNode matNode = node.AddChild("material");
            material.Serialize(matNode);
        }

        void Deserialize(const XMLNode &node) override
        {
            node.GetAttribute("name", name);
            node.GetAttribute("enabled", enabled);
            node.GetAttribute("mesh", meshSourcePath); // caller loads the asset

            XMLNode tNode = node.GetChild("transform");
            if (tNode.IsValid())
                transform.Deserialize(tNode);

            XMLNode matNode = node.GetChild("material");
            if (matNode.IsValid())
                material.Deserialize(matNode);
        }

        std::string meshSourcePath; // e.g. "assets/meshes/cube.obj"
    };

    struct SceneLight : public Serializable
    {
        std::string name;
        TransformComponent transform;
        Light light;

        void Serialize(XMLNode &node) const override
        {
            node.SetAttribute("name", name);

            XMLNode tNode = node.AddChild("transform");
            transform.Serialize(tNode);

            XMLNode lNode = node.AddChild("light");
            light.Serialize(lNode); // implement similarly
        }

        void Deserialize(const XMLNode &node) override
        {
            node.GetAttribute("name", name);

            XMLNode tNode = node.GetChild("transform");
            if (tNode.IsValid())
                transform.Deserialize(tNode);

            XMLNode lNode = node.GetChild("light");
            if (lNode.IsValid())
                light.Deserialize(lNode);
        }
    };
}