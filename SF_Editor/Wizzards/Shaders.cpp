#pragma once
#include "Shaders.hpp"
#include <Gui/GuiMembers.hpp>
#include <LowLevel/Filesystem/File.hpp>
#include "../Panels/AssetsWindow.hpp"
#include <Rendering/Shaders/ShaderAsset.hpp>
#include "../Panels/Panels.hpp"

namespace SF::Engine
{
    // todo: update asset manifests
    void CreateShaderWithStages(std::vector<std::string> stages, std::filesystem::path path, std::string name)
    {
        File shader(path);
        FileWriter writer(shader);

        writer << "// Define bindings like: Input(num,set) Sampler2D...\n";
        writer << "// Ex: Input(1,0) Sampler2D<float4> inMyTex;\n";
        // make file and insert default slang stuff
        for (auto stage : stages)
        {
            if (stage.c_str() == "Vertex")
            {
                writer << "#include \"ShaderCommon.si\"\n";
                writer << "\n";
                writer << "[shader(\"vertex\")]\n";
                writer << "VSOutput VertexShader(VSInput in)\n";
                writer << "{\n";
                writer << "    VSOutput output = RunVertexShader(in);\n";
                writer << "    return output;\n";
                writer << "}\n";
                writer << "\n";
            }
            else if (stage.c_str() == "Fragment")
            {
                writer << "[shader(\"fragment)\")]\n";
                writer << "FSOutput FragmentShader(VSOutput in)\n";
                writer << "{\n";
                writer << "    FSOutput out;\n";
                writer << "    // Your Logic Here\n";
                writer << "    return out\n;";
                writer << "}\n";
                writer << "\n";
            }
            else if (stage.c_str() == "Compute")
            {
                writer << "[numthreads(8,8,1)] // Replace with your thread group counts\n";
                writer << "[shader(\"compute)\")]\n";
                writer << "void ComputeShader(uint3 globalThreadID : SV_DispatchThreadID)\n";
                writer << "{\n";
                writer << "    // Your Logic\n";
                writer << "}\n";
                writer << "\n";
            }
            // TODO: add TessEval/TessControl to here and ShaderCommon.si
        }
        shader.Close();

        auto shaderAsset = AssetController::Get()->RegisterAsset<ShaderAsset>(name);
        shaderAsset->type = AssetType::Shader;
        shaderAsset->assetPath = path;
        shaderAsset->SaveMeta(); // writes <path>.meta only — no full-manifest rewrite

        showCS = false;
    }
    std::vector<std::string> stages;
    std::string name;
    std::string tmp = "New Shader";

    bool hasV;
    bool hasF;
    bool hasTE;
    bool hasTC;
    // same for shader includes but without stages
    void ShowCreateShaderWizzard(std::filesystem::path path)
    {
        ImGui::BeginPopup("Create Shader");

        if (InputTextWithHint("##Name", &tmp, &name))
        {
            path = path / std::string(name + ".shader");
        }
        if (ImGui::Checkbox("Vertex", &hasV))
        {
            stages.push_back("Vertex");
        }
        if (ImGui::Checkbox("Fragment", &hasF))
        {
            stages.push_back("Fragment");
        }
        if (ImGui::Checkbox("Tesselation Control", &hasTC))
        {
            stages.push_back("TessCtrl");
        }
        if (ImGui::Checkbox("Tesselation Evaluation", &hasTE))
        {
            stages.push_back("TessEval");
        }

        if (ImGui::Button("Add Compute Shader"))
        {
            stages.push_back("Compute");
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove Compute Shader"))
        {
            // FIXME:
            // auto remove = std::find_first_of(stages.begin(), stages.end(), std::string("Compute").begin(), std::string("Compute").end());
            // stages.erase(remove);
        }

        if (ImGui::Button("Create"))
            CreateShaderWithStages(stages, path, name);

        ImGui::EndPopup();
    }

    void CreateShaderInclude(std::filesystem::path path, std::string incGaurdName)
    {
        File inc(path);
        FileWriter writer(inc);
        writer << "#ifndef " + incGaurdName + "_INCLUDE\n";
        writer << "#define " + incGaurdName + "_INCLUDE\n";
        writer << "\n";
        writer << "// Your Logic\n";
        writer << "\n";
        writer << "#endif   \n";
        inc.Close();
        showCSI = false;
    }

    void ShowCreateShaderIncludeWizzard(std::filesystem::path path)
    {
        ImGui::BeginPopup("Create Shader");

        if (InputTextWithHint("##Name2", &tmp, &name))
        {
            path = path / std::string(name + ".si");
        }

        if (ImGui::Button("Create"))
            CreateShaderInclude(path, name);
        ImGui::EndPopup();
    }
}
