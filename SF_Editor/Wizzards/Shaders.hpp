#pragma once
#include <filesystem>
#include <Gui/GuiMembers.hpp>
#include <TemplateLibrary/DynamicArray.hpp>

namespace SF::Engine
{
    void CreateShaderWithStages(::SFTL::DynamicArray<std::string> stages, std::filesystem::path path, std::string name)
    {
        // make file and insert default slang stuff
    }
    void ShowCreateShaderWizzard(std::filesystem::path path)
    {
        ::SFTL::DynamicArray<std::string> stages;

        if(ImGui::Begin("Create Shader"))
        {   
            std::string name;

            bool hasV;
            bool hasF;
            bool hasC;
            bool hasTE;
            bool hasTC;


            if(InputTextWithHint("##Name", &std::string("New Shader"), &name))
            {
                // idk
            }
            if(ImGui::CheckBox("Vertex"), &hasV)
            {
                stages.emplace_back("Vertex");
            }
            if(ImGui::CheckBox("Fragment"), &hasF)
            {
                stages.emplace_back("Fragment");
            }
            if(ImGui::CheckBox("Compute"), &hasC)
            {
                stages.emplace_back("Compute");
            }
            if(ImGui::CheckBox("Tesselation Control"), &hasTC)
            {
                stages.emplace_back("TessCtrl");
            }
            if(ImGui::CheckBox("Tesselation Evaluation"), &hasTE)
            {
                stages.emplace_back("TessEval");
            }
            
            if(ImGui::Button("Create")) CreateShaderWithStages(stages, path, name);

            ImGui::End();
        }
        
    }
}