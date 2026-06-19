#pragma once
#include <ImGui/ocornut/imgui.h>

namespace SF::Engine
{
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

    static bool InputTextWithHint(const char *id, const char *hint,
                                  char *buf, size_t bufSize,
                                  ImGuiInputTextFlags flags = 0)
    {
        bool changed = ImGui::InputText(id, buf, bufSize, flags);
        if (buf[0] == '\0' && !ImGui::IsItemActive())
        {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetItemRectMin();
            pos.x += ImGui::GetStyle().FramePadding.x;
            pos.y += ImGui::GetStyle().FramePadding.y;
            dl->AddText(pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), hint);
        }
        return changed;
    }

}