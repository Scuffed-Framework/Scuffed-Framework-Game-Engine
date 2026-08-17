#include "BarPanels.hpp"
#include <Engine/Engine.hpp>
#include <Scene/SceneManager.hpp>
#include <Controllers/CameraController.hpp>
#include <algorithm>
#include <functional>
#include <Gui/GuiMembers.hpp>
#include <Project/Project.hpp>
#include <Engine/Version.hpp>
#include "Panels.hpp"

namespace SF::Engine
{
    void BarPanels::Draw()
    {
        DrawMenuBar();
        DrawEngineStatusBar();
    }

    void BarPanels::DrawMenuBar()
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::BeginMenu("Save"))
                {
                    if (ImGui::MenuItem("Save Scene"))
                    {
                        if (ProjectManager::Get()->IsAProjectLoaded())
                            printf("no"); // SceneManager::Get()->GetScene()->Serialize();
                    }
                    if (ImGui::MenuItem("Save Project"))
                    {
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("New"))
                {
                    if (ImGui::MenuItem("New Scene"))
                    {
                    }
                    if (ImGui::MenuItem("New Project"))
                    {
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Open"))
                {
                    if (ImGui::MenuItem("Open Scene"))
                    {
                    }
                    if (ImGui::MenuItem("Open Project"))
                    {
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Exit"))
                {
                    Engine::Get()->RequestClose();
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::MenuItem("Hierarchy", nullptr, &ShowHierarchy);
                ImGui::MenuItem("Inspector", nullptr, &ShowInspector);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Camera"))
            {
                auto *ec = static_cast<EditorCamera *>(CameraController::Get().GetActive());
                ImGui::InputFloat("Speed##cs", &ec->moveSpeed);
                ImGui::SliderFloat("Sensitivity##cs", &ec->lookSensitivity, 0.01f, 1.0f);
                ImGui::SliderFloat("FOV##cs", &ec->fovDeg, 20.0f, 120.0f);
                ImGui::Separator();
                ImGui::TextDisabled("Teleport");
                auto pos = CameraController::Get().GetActive()->GetPosition();
                // Y=0 in game space = sea level. Display altitude in metres below 10km, km above.
                float altM = pos.y;
                if (std::abs(altM) < 10000.0f)
                    ImGui::Text("Alt: %.0f m", altM);
                else
                    ImGui::Text("Alt: %.1f km", altM / 1000.0f);
                if (ImGui::Button("Surface (0 m)"))
                    CameraController::Get().GetActive()->SetPosition({0.0f, 10.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("10 km"))
                    CameraController::Get().GetActive()->SetPosition({0.0f, 10000.0f, 0.0f});
                if (ImGui::Button("50 km"))
                    CameraController::Get().GetActive()->SetPosition({0.0f, 50000.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("100 km"))
                    CameraController::Get().GetActive()->SetPosition({0.0f, 100000.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("200 km (orbit)"))
                    CameraController::Get().GetActive()->SetPosition({0.0f, 200000.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("500 km"))
                    CameraController::Get().GetActive()->SetPosition({0.0f, 500000.0f, 0.0f});
                ImGui::SameLine();
                if (ImGui::Button("1000 km"))
                    CameraController::Get().GetActive()->SetPosition({0.0f, 1000000.0f, 0.0f});
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

    void BarPanels::DrawEngineStatusBar()
    {
        auto &io = ImGui::GetIO();
        float barH = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        ImGui::SetNextWindowPos(
            {0, io.DisplaySize.y - barH}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            {io.DisplaySize.x, barH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.55f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 3));
        ImGui::Begin("##statusbar", nullptr,
                        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
                            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        
        ImGui::Text("SF Engine Version: %*s", (int)Engine_VERSION.length(), Engine_VERSION.data());
        HorizontalSpacer(40);
        if(ProjectManager::Get()->IsAProjectLoaded())
            ImGui::Text((std::string("Project: ") + ProjectManager::Get()->GetCurrentProject()->name).c_str());
        
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::End();
        ImGui::PopStyleVar();
    }
}