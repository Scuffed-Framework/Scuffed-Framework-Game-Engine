#include "Shaders.hpp"
#include <Gui/ImGui/GuiMembers.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <Rendering/RHI/Shaders/ShaderAsset.hpp>
#include <utility>
#include "../Panels/AssetsWindow.hpp"
#include "../Panels/Panels.hpp"

namespace SF::Engine
{
    void CreateShaderWithStages(const std::vector<std::string> &stages, const std::filesystem::path &path,
                                const std::string &name)
    {
        File shader(path);
        FileWriter writer(shader);

        writer << "// Define bindings like: Input(num,set) Sampler2D...\n";
        writer << "// Ex: Input(1,0) Sampler2D<float4> inMyTex;\n";
        // make file and insert default slang stuff
        for (const auto &stage: stages)
        {
            if (stage == "Vertex")
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
            } else if (stage == "Fragment")
            {
                writer << "[shader(\"fragment)\")]\n";
                writer << "FSOutput FragmentShader(VSOutput in)\n";
                writer << "{\n";
                writer << "    FSOutput out;\n";
                writer << "    // Your Logic Here\n";
                writer << "    return out\n;";
                writer << "}\n";
                writer << "\n";
            } else if (stage == "Compute")
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

        auto shaderAsset       = AssetController::Get()->RegisterAsset<ShaderAsset>(name);
        shaderAsset->type      = AssetType::Shader;
        shaderAsset->assetPath = path;
        shaderAsset->SaveMeta(); // writes <path>.meta only — no full-manifest rewrite

        showCS = false;
    }
    namespace
    {
        std::vector<std::string> stages;
        std::string name;
        std::string tmp = "New Shader";

        bool hasV       = false;
        bool hasF       = false;
        bool hasTE      = false;
        bool hasTC      = false;
        bool hasCompute = false;
    } // namespace

    void ShowCreateShaderWizard(std::filesystem::path path)
    {
        ImGui::BeginPopup("Create Shader");

        if (InputTextWithHint("##Name", &tmp, &name))
            path = path / (name + ".shader");

        auto ToggleStage = [](std::vector<std::string> &stages, const char *stage, bool enabled)
        {
            if (const auto it = std::ranges::find(stages, stage); enabled && it == stages.end())
                stages.emplace_back(stage);
            else if (!enabled && it != stages.end())
                stages.erase(it);
        };

        if (ImGui::Checkbox("Vertex", &hasV))
            ToggleStage(stages, "Vertex", hasV);

        if (ImGui::Checkbox("Fragment", &hasF))
            ToggleStage(stages, "Fragment", hasF);

        if (ImGui::Checkbox("Tesselation Control", &hasTC))
            ToggleStage(stages, "TessCtrl", hasTC);

        if (ImGui::Checkbox("Tesselation Evaluation", &hasTE))
            ToggleStage(stages, "TessEval", hasTE);

        if (ImGui::Checkbox("Compute", &hasCompute))
            ToggleStage(stages, "Compute", hasCompute);

        if (ImGui::Button("Create"))
            CreateShaderWithStages(stages, path, name);

        ImGui::EndPopup();
    }

    void CreateShaderInclude(std::filesystem::path path)
    {
        File inc(std::move(path));
        FileWriter writer(inc);
        writer << "#pragma once\n";
        writer << "\n";
        writer << "// Your Logic Here\n";
        writer << "\n";
        writer << "#endif   \n";
        inc.Close();
        showCSI = false;
    }

    void ShowCreateShaderIncludeWizard(std::filesystem::path path)
    {
        ImGui::BeginPopup("Create Shader");

        if (InputTextWithHint("##Name2", &tmp, &name))
        {
            path = path / std::string(name + ".si");
        }

        if (ImGui::Button("Create"))
            CreateShaderInclude(path);
        ImGui::EndPopup();
    }
} // namespace SF::Engine
