#include "ImGuiDefaultWIDGETS.hpp"
#include <ImGui/ocornut/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>

namespace SF::Engine
{

    //
    // Internal helpers
    //

    // Accent colours matching the engine theme (blue accent from ImGuiDefault)
    static constexpr ImVec4 kAccent = {0.00f, 0.54f, 1.00f, 1.00f};
    static constexpr ImVec4 kAccentDim = {0.00f, 0.54f, 1.00f, 0.20f};
    static constexpr ImVec4 kRed = {0.86f, 0.20f, 0.20f, 1.00f};
    static constexpr ImVec4 kRedDim = {0.86f, 0.20f, 0.20f, 0.25f};
    static constexpr ImVec4 kGreen = {0.20f, 0.80f, 0.20f, 1.00f};
    static constexpr ImVec4 kGreenDim = {0.20f, 0.80f, 0.20f, 0.25f};
    static constexpr ImVec4 kBlue = {0.20f, 0.40f, 1.00f, 1.00f};
    static constexpr ImVec4 kBlueDim = {0.20f, 0.40f, 1.00f, 0.25f};
    static constexpr ImVec4 kComponentBg = {0.08f, 0.08f, 0.08f, 0.80f};
    static constexpr ImVec4 kHeaderBg = {0.12f, 0.12f, 0.14f, 1.00f};

    // Draws a thin vertical coloured bar then a bold label, returns CollapsingHeader result.
    bool SectionHeader(const char *label, bool *visible)
    {
        ImGui::PushStyleColor(ImGuiCol_Header, kHeaderBg);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.22f, 0.22f, 0.28f, 1.0f));

        // Draw a coloured left stripe
        ImVec2 p = ImGui::GetCursorScreenPos();
        float h = ImGui::GetFrameHeight();
        ImGui::GetWindowDrawList()->AddRectFilled(
            p, ImVec2(p.x + 3.0f, p.y + h), IM_COL32(0, 138, 255, 200));
        ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
        bool open = ImGui::CollapsingHeader(label,
                                            visible ? ImGuiTreeNodeFlags_AllowOverlap : ImGuiTreeNodeFlags_None);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        // Eye / enable toggle in the header right side
        if (visible)
        {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 16.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
            char eid[64];
            std::snprintf(eid, sizeof(eid), "##vis_%s", label);
            if (ImGui::SmallButton(*visible ? "O" : "-"))
                *visible = !*visible;
            ImGui::PopStyleColor(2);
        }

        return open;
    }

    // Property label in left column : call after BeginTable(2 cols).
    void PropLabel(const char *label)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", label);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN); // fill remaining width
    }

    //  DragFloat row with X/Y/Z coloured buttons
    static void Vec3Row(const char *id, float *v,
                        float resetX, float resetY, float resetZ,
                        float speed = 0.1f, const char *fmt = "%.3f")
    {
        float btnW = ImGui::GetFrameHeight(); // square buttons

        // X
        ImGui::PushStyleColor(ImGuiCol_Button, kRedDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kRed);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kRed);
        char bid[32];
        std::snprintf(bid, sizeof(bid), "X##%s", id);
        if (ImGui::Button(bid, ImVec2(btnW, 0)))
            v[0] = resetX;
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 1);
        ImGui::PushItemWidth((ImGui::GetContentRegionAvail().x - btnW * 2 - 4.0f) / 3.0f);
        char did[32];
        std::snprintf(did, sizeof(did), "##vx_%s", id);
        ImGui::DragFloat(did, &v[0], speed, 0, 0, fmt);
        ImGui::PopItemWidth();

        // Y
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Button, kGreenDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kGreen);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kGreen);
        std::snprintf(bid, sizeof(bid), "Y##%s", id);
        if (ImGui::Button(bid, ImVec2(btnW, 0)))
            v[1] = resetY;
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 1);
        ImGui::PushItemWidth((ImGui::GetContentRegionAvail().x - btnW - 4.0f) / 2.0f);
        std::snprintf(did, sizeof(did), "##vy_%s", id);
        ImGui::DragFloat(did, &v[1], speed, 0, 0, fmt);
        ImGui::PopItemWidth();

        // Z
        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Button, kBlueDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlue);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, kBlue);
        std::snprintf(bid, sizeof(bid), "Z##%s", id);
        if (ImGui::Button(bid, ImVec2(btnW, 0)))
            v[2] = resetZ;
        ImGui::PopStyleColor(3);
        ImGui::SameLine(0, 1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        std::snprintf(did, sizeof(did), "##vz_%s", id);
        ImGui::DragFloat(did, &v[2], speed, 0, 0, fmt);
    }

    //
    // TransformWidget
    //

    void TransformWidget::Draw(TransformComponent &t, const char *id)
    {
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kComponentBg);
        ImGui::BeginChild("##TfBody", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

        if (SectionHeader("Transform"))
        {
            ImGui::Spacing();
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));
            if (ImGui::BeginTable("##TfTbl", 2,
                                  ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                // Position
                PropLabel("Position");
                Vec3Row("Pos", glm::value_ptr(t.position), 0, 0, 0, 0.05f);

                // Rotation
                PropLabel("Rotation");
                Vec3Row("Rot", glm::value_ptr(t.rotation), 0, 0, 0, 0.5f, "%.1f");

                // Scale
                PropLabel("Scale");
                Vec3Row("Scl", glm::value_ptr(t.scale), 1, 1, 1, 0.01f);

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();

            // Reset button
            ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 52.0f + ImGui::GetCursorPosX());
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));
            if (ImGui::SmallButton("Reset##Tf"))
                t.Reset();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    //
    // MaterialWidget
    //

    void MaterialWidget::Draw(MeshMaterial &mat, const char *id)
    {
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kComponentBg);
        ImGui::BeginChild("##MatBody", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

        if (SectionHeader("Material"))
        {
            ImGui::Spacing();
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));

            if (ImGui::BeginTable("##MatTbl", 2,
                                  ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                // Albedo colour (inline swatch + RGB sliders like Unity)
                PropLabel("Albedo");
                ImGui::ColorEdit4("##Albedo", glm::value_ptr(mat.baseColor),
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float |
                                      ImGuiColorEditFlags_PickerHueBar);

                // Metallic
                PropLabel("Metallic");
                ImGui::SliderFloat("##Metal", &mat.metallicFactor, 0.0f, 1.0f, "%.2f");

                // Roughness (displayed as "Smoothness" with inverse, like Unity)
                PropLabel("Roughness");
                ImGui::SliderFloat("##Rough", &mat.roughnessFactor, 0.0f, 1.0f, "%.2f");

                // AO
                PropLabel("AO");
                ImGui::SliderFloat("##AO", &mat.aoFactor, 0.0f, 1.0f, "%.2f");

                // Emissive intensity
                PropLabel("Emission");
                ImGui::SliderFloat("##Emis", &mat.emissiveFactor, 0.0f, 4.0f, "%.2f");

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();

            // Preview row: coloured rect showing the base colour
            ImVec2 swatchSz{ImGui::GetContentRegionAvail().x, 8.0f};
            ImVec2 cp = ImGui::GetCursorScreenPos();
            ImU32 col = IM_COL32(
                (int)(mat.baseColor.r * 255),
                (int)(mat.baseColor.g * 255),
                (int)(mat.baseColor.b * 255), 255);
            ImGui::GetWindowDrawList()->AddRectFilled(cp,
                                                      ImVec2(cp.x + swatchSz.x, cp.y + swatchSz.y), col);
            ImGui::Dummy(swatchSz);
            ImGui::Spacing();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    //
    // LightWidget
    //

    void LightWidget::Draw(Light &light, const char *id)
    {
        ImGui::PushID(id);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, kComponentBg);
        ImGui::BeginChild("##LightBody", ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

        if (SectionHeader("Light"))
        {
            ImGui::Spacing();
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));

            if (ImGui::BeginTable("##LightTbl", 2,
                                  ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                // Type
                PropLabel("Type");
                {
                    const char *typeNames[] = {"Point", "Spot", "Directional"};
                    int typeIdx = static_cast<int>(light.type);
                    if (ImGui::Combo("##LType", &typeIdx, typeNames, 3))
                        light.type = static_cast<Lighting::LightType>(typeIdx);
                }

                // Colour
                PropLabel("Color");
                ImGui::ColorEdit3("##LCol", glm::value_ptr(light.color),
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float |
                                      ImGuiColorEditFlags_PickerHueBar);

                // Intensity
                PropLabel("Intensity");
                ImGui::DragFloat("##LInt", &light.intensity, 0.1f, 0.0f, 500.0f, "%.2f");

                // Range (point / spot only)
                if (light.type != Lighting::LightType::Directional)
                {
                    PropLabel("Range");
                    ImGui::DragFloat("##LRad", &light.radius, 0.1f, 0.1f, 500.0f, "%.2f");
                }

                // Spot angles
                if (light.type == Lighting::LightType::Spot)
                {
                    PropLabel("Inner Angle");
                    ImGui::SliderFloat("##LInner", &light.innerConeAngleDeg,
                                       0.0f, light.outerConeAngleDeg, "%.1f°");

                    PropLabel("Outer Angle");
                    ImGui::SliderFloat("##LOuter", &light.outerConeAngleDeg,
                                       light.innerConeAngleDeg, 180.0f, "%.1f°");
                }

                // Shadow toggle
                PropLabel("Cast Shadow");
                ImGui::Checkbox("##LShadow", &light.castShadow);

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            ImGui::Spacing();

            // Intensity preview bar
            ImVec2 cp = ImGui::GetCursorScreenPos();
            float barW = ImGui::GetContentRegionAvail().x;
            float fillW = barW * glm::clamp(light.intensity / 50.0f, 0.0f, 1.0f);
            ImU32 bgCol = IM_COL32(30, 30, 30, 255);
            ImU32 fgCol = IM_COL32(
                (int)(light.color.r * 220),
                (int)(light.color.g * 220),
                (int)(light.color.b * 220), 200);
            ImGui::GetWindowDrawList()->AddRectFilled(cp, ImVec2(cp.x + barW, cp.y + 4), bgCol, 2);
            ImGui::GetWindowDrawList()->AddRectFilled(cp, ImVec2(cp.x + fillW, cp.y + 4), fgCol, 2);
            ImGui::Dummy(ImVec2(barW, 4));
            ImGui::Spacing();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    //
    // Legacy wrapper
    //

    void ImGuiWidgets::TransformsWidget()
    {
        static TransformComponent t;
        TransformWidget::Draw(t, "##LegacyTransform");
    }

} // namespace SF::Engine
