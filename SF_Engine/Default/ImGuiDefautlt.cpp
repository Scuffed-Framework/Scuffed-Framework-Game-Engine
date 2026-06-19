#include "ImGuiDefault.hpp"

namespace SF::Engine
{
    void ImGuiDefaultStyle::SetStyle()
    {
        ImGuiStyle &style = ImGui::GetStyle();

        style.ColorButtonPosition = ImGuiDir_Right;

        style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.0, 0.54, 1.0f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.0, 0.54, 1.0f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.0, 0.64, 1.0f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.0, 0.58, 1.0f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.1f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.5f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.5f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 1.00f, 0.00f, 0.50f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 1.00f, 0.00f, 0.50f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.00f, 1.00f, 0.00f, 0.50f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.00f, 1.00f, 0.00f, 0.50f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.2f, 0.2f, 0.2f, 0.50f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 0.1f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 0.5f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.25f, 0.25f, 0.25f, 0.5f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.4f, 0.4f, 0.4f, 0.5f);

        style.FrameRounding = 0;
        style.WindowBorderSize = 0.1f;
    }
}
