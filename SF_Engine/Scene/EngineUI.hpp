#include <ImGui/ocornut/imgui.h>
#include <string>
#include <vector>
#include <Camera/EditorCamera.hpp>
#include <cstdio>

#include <Scene/Types.hpp>
#include <Components/TransformComponent.hpp>
#include <Default/ImGuiDefaultWIDGETS.hpp>

namespace SF::Engine
{
    class ACamera;

    class EngineUI
    {
    public:
        bool showHierarchy_ = true;
        bool showInspector_ = true;
        bool showLights_ = false;

        struct DrawContext
        {
            ACamera *camera;
            std::vector<SceneObject> *objects;
            std::vector<SceneLight> *lights;
            int *selectedObj;
            int *selectedLight;
        };

    private:
        DrawContext ctx_{};

        void DrawMenuBar()
        {
            if (ImGui::BeginMainMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Exit"))
                    {
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

        // Small non-interactive status bar at the bottom with camera info +
        // control hint that disappears when RMB is held.
        void DrawCameraStatusBar()
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

            auto pos = ctx_.camera->GetPosition();
            char buf[128];
            ImGui::TextDisabled("%s", buf);

            ImGui::End();
            ImGui::PopStyleVar();
        }

        void DrawHierarchyPanel()
        {
            if (!showHierarchy_)
                return;
            ImGui::SetNextWindowPos({0.0f, 20.0f}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize({200.0f, 300.0f}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Hierarchy", &showHierarchy_, ImGuiWindowFlags_NoCollapse);

            for (int i = 0; i < (int)ctx_.objects->size(); i++)
            {
                auto &obj = (*ctx_.objects)[i];
                ImGuiTreeNodeFlags f =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanFullWidth |
                    (*ctx_.selectedObj == i ? ImGuiTreeNodeFlags_Selected : 0);
                if (!obj.enabled)
                    ImGui::PushStyleColor(ImGuiCol_Text, {0.5f, 0.5f, 0.5f, 1.0f});
                ImGui::TreeNodeEx(obj.name.c_str(), f);
                if (!obj.enabled)
                    ImGui::PopStyleColor();
                if (ImGui::IsItemClicked())
                {
                    *ctx_.selectedObj = i;
                    *ctx_.selectedLight = -1;
                }
            }

            ImGui::Separator();
            ImGui::TextDisabled("Lights");
            for (int i = 0; i < (int)ctx_.lights->size(); i++)
            {
                auto &sl = (*ctx_.lights)[i];
                const char *prefix =
                    sl.light.type == SF::Engine::Lighting::LightType::Directional ? "[D] " : sl.light.type == SF::Engine::Lighting::LightType::Spot ? "[S] "
                                                                                                                                                    : "[P] ";
                std::string label = prefix + sl.name;
                ImGuiTreeNodeFlags f =
                    ImGuiTreeNodeFlags_Leaf |
                    ImGuiTreeNodeFlags_NoTreePushOnOpen |
                    ImGuiTreeNodeFlags_SpanFullWidth |
                    (*ctx_.selectedLight == i ? ImGuiTreeNodeFlags_Selected : 0);
                ImGui::TreeNodeEx(label.c_str(), f);
                if (ImGui::IsItemClicked())
                {
                    *ctx_.selectedLight = i;
                    *ctx_.selectedObj = -1;
                }
            }
            ImGui::End();
        }

        void DrawInspectorPanel()
        {
            if (!showInspector_)
                return;
            ImGui::SetNextWindowPos(
                {ImGui::GetIO().DisplaySize.x - 290.0f, 20.0f}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize({285.0f, 600.0f}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Inspector", &showInspector_, ImGuiWindowFlags_NoCollapse);

            if (*ctx_.selectedObj >= 0 && *ctx_.selectedObj < (int)ctx_.objects->size())
            {
                auto &obj = (*ctx_.objects)[*ctx_.selectedObj];
                ImGui::Checkbox("##En", &obj.enabled);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                char nb[64];
                std::snprintf(nb, sizeof(nb), "%s", obj.name.c_str());
                if (ImGui::InputText("##ObjName", nb, sizeof(nb)))
                    obj.name = nb;
                ImGui::Separator();

                ImGui::Spacing();

                SF::Engine::TransformWidget::Draw(obj.transform, "ObjTf");
                ImGui::Spacing();
                SF::Engine::MaterialWidget::Draw(obj.material, "ObjMat");
            }
            else if (*ctx_.selectedLight >= 0 && *ctx_.selectedLight < (int)ctx_.lights->size())
            {
                auto &sl = (*ctx_.lights)[*ctx_.selectedLight];
                ImGui::TextColored({1.0f, 0.85f, 0.3f, 1.0f}, "Light Object");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                char nb[64];
                std::snprintf(nb, sizeof(nb), "%s", sl.name.c_str());
                if (ImGui::InputText("##LName", nb, sizeof(nb)))
                {
                    sl.name = nb;
                    sl.light.name = nb;
                }
                ImGui::Separator();
                ImGui::Spacing();
                SF::Engine::TransformWidget::Draw(sl.transform, "LightTf");
                ImGui::Spacing();
                SF::Engine::LightWidget::Draw(sl.light, "LightL");
            }
            else
            {
                ImGui::TextDisabled("Nothing selected");
                ImGui::Spacing();
                ImGui::TextDisabled("Click an item in the Hierarchy.");
            }
            ImGui::End();
        }

        void DrawLightsPanel()
        {
            if (!showLights_)
                return;
            ImGui::SetNextWindowPos({210.0f, 20.0f}, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize({300.0f, 500.0f}, ImGuiCond_FirstUseEver);
            ImGui::Begin("Lights", &showLights_, ImGuiWindowFlags_NoCollapse);
            ImGui::TextDisabled("%zu light(s)", ctx_.lights->size());
            ImGui::Separator();
            ImGui::Spacing();
            for (int i = 0; i < (int)(*ctx_.lights).size(); i++)
            {
                auto &sl = (*ctx_.lights)[i];
                ImGui::PushID(i);
                if (ImGui::CollapsingHeader(sl.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Indent(8.0f);
                    ImVec4 sw(sl.light.color.r, sl.light.color.g, sl.light.color.b, 1.0f);
                    ImGui::ColorButton("##sw", sw,
                                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
                                       {12, 12});
                    ImGui::SameLine();
                    ImGui::TextUnformatted(sl.name.c_str());
                    ImGui::Spacing();
                    SF::Engine::TransformWidget::Draw(sl.transform,
                                                      ("LT" + std::to_string(i)).c_str());
                    ImGui::Spacing();
                    SF::Engine::LightWidget::Draw(sl.light,
                                                  ("LL" + std::to_string(i)).c_str());
                    ImGui::Unindent(8.0f);
                }
                ImGui::Spacing();
                ImGui::PopID();
            }
            ImGui::End();
        }

    public:
        void Draw(const DrawContext &ctx)
        {
            ctx_ = ctx;

            DrawMenuBar();
            DrawHierarchyPanel();
            DrawInspectorPanel();
            DrawLightsPanel();
            DrawCameraStatusBar();
        }
    };
}
