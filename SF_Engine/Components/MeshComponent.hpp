#pragma once

#include <string>
#include <memory>
#include <Graphics/Mesh/Mesh.hpp>
#include <Graphics/Lighting/LitMeshPipelinePass.hpp>
#include <XML/XMLReader.hpp>

namespace SF::Engine
{
    struct MeshComponent : public Serializable
    {
        std::shared_ptr<Mesh> mesh;
        MeshMaterial material;
        std::string meshSourcePath; // e.g. "assets/meshes/cube.obj"

        void Serialize(XMLNode &node) const override
        {
            node.SetAttribute("mesh", meshSourcePath);
            XMLNode matNode = node.AddChild("material");
            material.Serialize(matNode);
        }

        void Deserialize(const XMLNode &node) override
        {
            node.GetAttribute("mesh", meshSourcePath); // caller (AssetController) loads `mesh`
            XMLNode matNode = node.GetChild("material");
            if (matNode.IsValid())
                material.Deserialize(matNode);
        }
    };

} // namespace SF::Engine