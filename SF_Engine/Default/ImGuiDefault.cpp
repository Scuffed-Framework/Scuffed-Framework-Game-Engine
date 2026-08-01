#include "ImGuiDefault.hpp"

namespace SF::Engine
{
    void ImGuiDefaultStyle::SetStyle2()
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

    void ImGuiDefaultStyle::SetStyle()
    {
        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = style.Colors;

        // --- Layout: flat, sharp, tight ---
        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.TabRounding = 0.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;

        style.WindowPadding = ImVec2(8, 8);
        style.FramePadding = ImVec2(6, 4);
        style.ItemSpacing = ImVec2(6, 6);
        style.ItemInnerSpacing = ImVec2(6, 4);
        style.IndentSpacing = 14.0f;
        style.ScrollbarSize = 12.0f;
        style.GrabMinSize = 10.0f;

        style.ColorButtonPosition = ImGuiDir_Right;

        const ImVec4 accent = ImVec4(0.00f, 0.54f, 1.00f, 1.00f); // signature blue
        const ImVec4 accentHover = ImVec4(0.00f, 0.62f, 1.00f, 1.00f);
        const ImVec4 accentActive = ImVec4(0.00f, 0.68f, 1.00f, 1.00f);

        const ImVec4 bgVoid = ImVec4(0.00f, 0.00f, 0.00f, 0.92f);  // window/popup bg
        const ImVec4 bgPanel = ImVec4(0.06f, 0.06f, 0.06f, 1.00f); // title bars
        const ImVec4 bgField = ImVec4(0.11f, 0.11f, 0.11f, 1.00f); // frame bg
        const ImVec4 bgFieldHover = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
        const ImVec4 bgFieldActive = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        const ImVec4 borderCol = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
        const ImVec4 rowSelect = ImVec4(0.20f, 0.20f, 0.20f, 1.00f); // hierarchy selection grey
        const ImVec4 rowHover = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
        const ImVec4 textDim = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);

        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = textDim;
        colors[ImGuiCol_WindowBg] = bgVoid;
        colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_PopupBg] = bgVoid;
        colors[ImGuiCol_Border] = borderCol;
        colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

        colors[ImGuiCol_FrameBg] = bgField;
        colors[ImGuiCol_FrameBgHovered] = bgFieldHover;
        colors[ImGuiCol_FrameBgActive] = bgFieldActive;

        colors[ImGuiCol_TitleBg] = bgPanel;
        colors[ImGuiCol_TitleBgActive] = bgPanel;
        colors[ImGuiCol_TitleBgCollapsed] = bgPanel;
        colors[ImGuiCol_MenuBarBg] = bgPanel;

        colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        colors[ImGuiCol_ScrollbarGrab] = bgFieldHover;
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);

        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accentActive;

        colors[ImGuiCol_Button] = bgField;
        colors[ImGuiCol_ButtonHovered] = bgFieldHover;
        colors[ImGuiCol_ButtonActive] = bgFieldActive;

        colors[ImGuiCol_Header] = rowSelect;
        colors[ImGuiCol_HeaderHovered] = rowHover;
        colors[ImGuiCol_HeaderActive] = rowSelect;

        colors[ImGuiCol_Separator] = borderCol;
        colors[ImGuiCol_SeparatorHovered] = accent;
        colors[ImGuiCol_SeparatorActive] = accentActive;

        colors[ImGuiCol_ResizeGrip] = accent;
        colors[ImGuiCol_ResizeGripHovered] = accentHover;
        colors[ImGuiCol_ResizeGripActive] = accentActive;

        colors[ImGuiCol_Tab] = bgPanel;
        colors[ImGuiCol_TabHovered] = bgFieldHover;
        colors[ImGuiCol_TabActive] = bgField;
        colors[ImGuiCol_TabUnfocused] = bgPanel;
        colors[ImGuiCol_TabUnfocusedActive] = bgField;

        colors[ImGuiCol_PlotLines] = accent;
        colors[ImGuiCol_PlotLinesHovered] = accentHover;
        colors[ImGuiCol_PlotHistogram] = accent;
        colors[ImGuiCol_PlotHistogramHovered] = accentHover;

        colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
        colors[ImGuiCol_DragDropTarget] = accent;
        colors[ImGuiCol_NavHighlight] = accent;
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    }
}
