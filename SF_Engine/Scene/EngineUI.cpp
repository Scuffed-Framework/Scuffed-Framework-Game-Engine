#include "EngineUI.hpp"
#include <Engine/Engine.hpp>
#include <Scene/SceneManager.hpp>

namespace SF::Engine
{
    void EngineUI::DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::BeginMenu("Save"))
                {
                    if (ImGui::MenuItem("Save Scene"))
                    {
                        auto scene = SceneManager::Get()->GetScene();
                        // SceneManager::Get()->GetScene()->Serialize();
                    }
                    if (ImGui::MenuItem("Save Project"))
                    {
                    }
                }
                ImGui::EndMenu();
                if (ImGui::BeginMenu("New"))
                {
                    if (ImGui::MenuItem("New Scene"))
                    {
                    }
                    if (ImGui::MenuItem("New Project"))
                    {
                    }
                }
                ImGui::EndMenu();
                if (ImGui::BeginMenu("Open"))
                {
                    if (ImGui::MenuItem("Open Scene"))
                    {
                    }
                    if (ImGui::MenuItem("Open Project"))
                    {
                    }
                }
                ImGui::EndMenu();

                if (ImGui::MenuItem("Exit"))
                {
                    Engine::Get()->RequestClose();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy_);
                ImGui::MenuItem("Inspector", nullptr, &showInspector_);
                ImGui::MenuItem("Lights", nullptr, &showLights_);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Camera"))
            {
                auto *ec = static_cast<EditorCamera *>(ctx_.camera);
                ImGui::SliderFloat("Speed##cs", &ec->moveSpeed, 0.1f, 50000.0f);
                ImGui::SliderFloat("Sensitivity##cs", &ec->lookSensitivity, 0.01f, 1.0f);
                ImGui::SliderFloat("FOV##cs", &ec->fovDeg, 20.0f, 120.0f);
                ImGui::Separator();
                ImGui::TextDisabled("Teleport");
                auto pos = ctx_.camera->GetPosition();
                // Y=0 in game space = sea level. Display altitude in metres below 10km, km above.
                float altM = pos.y;
                if (std::abs(altM) < 10000.0f)
                    ImGui::Text("Alt: %.0f m", altM);
                else
                    ImGui::Text("Alt: %.1f km", altM / 1000.0f);
                if (ImGui::Button("Surface (0 m)"))
                    ctx_.camera->SetPosition({0.0f, 10.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("10 km"))
                    ctx_.camera->SetPosition({0.0f, 10000.0f, 0.0f});
                if (ImGui::Button("50 km"))
                    ctx_.camera->SetPosition({0.0f, 50000.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("100 km"))
                    ctx_.camera->SetPosition({0.0f, 100000.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("200 km (orbit)"))
                    ctx_.camera->SetPosition({0.0f, 200000.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("500 km"))
                    ctx_.camera->SetPosition({0.0f, 500000.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("1000 km"))
                    ctx_.camera->SetPosition({0.0f, 1000000.0f, 0.0f});
                ImGui::EndMenu();
            }

            // FPS right-aligned
            char fpsBuf[32];
            std::snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", ImGui::GetIO().Framerate);
            float fpsW = ImGui::CalcTextSize(fpsBuf).x + 16.0f;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - fpsW);
            ImGui::TextDisabled("%s", fpsBuf);
            ImGui::EndMainMenuBar();
        }
    }
}