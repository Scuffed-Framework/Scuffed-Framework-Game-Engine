#pragma once
#include <Gui/ocornut/imgui.h>
#include <Gui/ocornut/imgui_stdlib.h>

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

    inline static bool InputTextWithHint(const char *id, const char *hint,
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
    inline static bool InputTextWithHint(const char *id, const std::string *hint,
                                  std::string *str, ImGuiInputTextFlags flags = 0)
    {
        bool changed = ImGui::InputText(id, str, flags);
        if (str->empty() && !ImGui::IsItemActive())
        {
            ImDrawList *dl = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetItemRectMin();
            pos.x += ImGui::GetStyle().FramePadding.x;
            pos.y += ImGui::GetStyle().FramePadding.y;
            dl->AddText(pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), hint->c_str());
        }
        return changed;
    }
    inline static void HorizontalSpacer(float width)
    {
        ImGui::SameLine();
        ImGui::Dummy(ImVec2(width, 0.0f));
        ImGui::SameLine();
    }

    // Done this way because I think it is easier to read
    inline std::string InputStringPopup(std::string WindowName, std::string WindowComment, std::string FallBackName)
    {
        ImGui::SetNextWindowSize(ImVec2(450, 250));
        
        std::string retValue;
        std::string storageValue;

        if (ImGui::Begin(WindowName.c_str()))
        {
            ImGui::Text(WindowComment.c_str());
            if (InputTextWithHint("##Name", &FallBackName, &storageValue))
            {
                retValue = storageValue;
            }
            else
            {
                retValue = FallBackName;
            }
            ImGui::End();
        }

        return retValue;
    }
}