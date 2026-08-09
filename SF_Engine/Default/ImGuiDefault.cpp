#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGuiDefault.hpp"
#include <ImGui/ImGuizmoIncludes.hpp>

namespace SF::Engine
{
    void ImGuiDefaultStyle::SetStyle()
    {
        ImGui::StyleColorsDark();

        ImGuiStyle &style = ImGui::GetStyle();
        ImVec4 *colors = ImGui::GetStyle().Colors;

        colors[ImGuiCol_BorderShadow] = ImVec4(0.1f, 0.1f, 0.0f, 0.39f);
        {
            style.Colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            style.Colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 1.00f);
            style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
            style.Colors[ImGuiCol_FrameBg] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
            style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.67f, 0.67f, 0.67f, 1.00f);
            style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
            style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
            style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
            style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
            style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
            style.Colors[ImGuiCol_CheckMark] = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
            style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
            style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
            style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.67f, 0.67f, 0.67f, 1.00f);
            style.Colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
            style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.67f, 0.67f, 0.67f, 1.00);
            style.Colors[ImGuiCol_Separator] = style.Colors[ImGuiCol_Border];
            style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
            style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.29f, 0.30f, 0.31f, 1.00f);
            style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
            style.Colors[ImGuiCol_TabHovered] = ImVec4(0.33f, 0.34f, 0.36f, 1.00f);
            style.Colors[ImGuiCol_TabActive] = ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
            style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
            style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
            style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            style.Colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
            style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
            style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
            style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
            style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
            style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
            style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
            style.Colors[ImGuiCol_CheckMark] = ImVec4(0.96f, 0.96f, 0.96f, 1.00f);
            style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 1.0f, 1.0f, 1.00f);
            style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.00f);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, -1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(2.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.0f);

        colors[ImGuiCol_BorderShadow] = ImVec4(0.1f, 0.1f, 0.0f, 0.39f);
        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 1;
        style.TabBorderSize = 1;
        style.WindowRounding = 0;
        style.ChildRounding = 0;
        style.FrameRounding = 3;
        style.PopupRounding = 0;
        style.ScrollbarRounding = 0;
        style.GrabRounding = 0;
        style.LogSliderDeadzone = 0;
        style.TabRounding = 0;

        ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
        ImGui::GetIO().ConfigWindowsResizeFromEdges = true;

        style.AntiAliasedLines = true;
        style.WindowMenuButtonPosition = ImGuiDir_Right;
        style.PopupRounding = 3;

        style.WindowPadding = ImVec2(4, 4);
        style.FramePadding = ImVec2(6, 4);
        style.ItemSpacing = ImVec2(6, 2);

        style.ScrollbarSize = 18;

        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 1;

        style.WindowRounding = 3;
        style.ChildRounding = 3;
        style.FrameRounding = 0;
        style.ScrollbarRounding = 2;
        style.GrabRounding = 0;

        style.TabBorderSize = 0;
        style.TabRounding = 3;
        style.WindowRounding = 0.0f;

        ImGuizmo::Style &styleGizmo = ImGuizmo::GetStyle();
        styleGizmo.TranslationLineThickness = 3.0f;
        styleGizmo.TranslationLineArrowSize = 6.0f;
        styleGizmo.RotationLineThickness = 4.0f;
        styleGizmo.RotationOuterLineThickness = 4.0f;
        styleGizmo.ScaleLineThickness = 2.5f;
        styleGizmo.ScaleLineCircleSize = 5.0f;
        styleGizmo.HatchedAxisLineThickness = 6.0f;
        styleGizmo.CenterCircleSize = 2.5f;

        styleGizmo.Colors[ImGuizmo::DIRECTION_X] = ImVec4(1.0f, 0.21f, 0.23f, 0.9f);
        styleGizmo.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(0.60f, 0.9f, 0.067f, 0.9f);
        styleGizmo.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(0.184f, 0.218f, 0.98f, 0.9f);
        styleGizmo.Colors[ImGuizmo::PLANE_X] = ImVec4(0.99f, 0.2f, 0.23f, 0.6f);
        styleGizmo.Colors[ImGuizmo::PLANE_Y] = ImVec4(0.60f, 0.9f, 0.067f, 0.6f);
        styleGizmo.Colors[ImGuizmo::PLANE_Z] = ImVec4(0.184f, 0.218f, 0.98f, 0.6f);

        styleGizmo.Colors[ImGuizmo::SELECTION] = ImVec4(1.000f, 0.500f, 0.062f, 0.541f);
        styleGizmo.Colors[ImGuizmo::INACTIVE] = ImVec4(0.600f, 0.600f, 0.600f, 0.600f);
        styleGizmo.Colors[ImGuizmo::TRANSLATION_LINE] = ImVec4(0.666f, 0.666f, 0.666f, 0.666f);
        styleGizmo.Colors[ImGuizmo::SCALE_LINE] = ImVec4(0.250f, 0.250f, 0.250f, 1.000f);
        styleGizmo.Colors[ImGuizmo::ROTATION_USING_BORDER] = ImVec4(1.000f, 0.500f, 0.062f, 1.000f);
        styleGizmo.Colors[ImGuizmo::ROTATION_USING_FILL] = ImVec4(1.000f, 0.500f, 0.062f, 0.500f);
        styleGizmo.Colors[ImGuizmo::HATCHED_AXIS_LINES] = ImVec4(0.000f, 0.000f, 0.000f, 0.500f);
        styleGizmo.Colors[ImGuizmo::TEXT] = ImVec4(1.000f, 1.000f, 1.000f, 1.000f);
        styleGizmo.Colors[ImGuizmo::TEXT_SHADOW] = ImVec4(0.000f, 0.000f, 0.000f, 1.000f);

        ImGuizmo::AllowAxisFlip(false);

        colors[ImGuiCol_BorderShadow] = ImVec4(0.1f, 0.1f, 0.0f, 0.39f);
        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 1;
        style.TabBorderSize = 1;
        style.WindowRounding = 0;
        style.ChildRounding = 0;
        style.FrameRounding = 1;
        style.PopupRounding = 0;
        style.ScrollbarRounding = 0;
        style.GrabRounding = 2;
        style.GrabMinSize = 8;
        style.LogSliderDeadzone = 0;
        style.TabRounding = 0;
        style.Alpha = 1.0f;
    }
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

    void ImGuiDefaultStyle::SetStyle3()
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

    void ImGuiDefaultStyle::SetStyle4()
    {
        ImVec4 *colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
        colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 0.67f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.44f, 0.44f, 0.44f, 0.40f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.46f, 0.47f, 0.48f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.42f, 0.42f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.70f, 0.70f, 0.70f, 0.31f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.70f, 0.70f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.48f, 0.50f, 0.52f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.72f, 0.72f, 0.72f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.91f, 0.91f, 0.91f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.81f, 0.81f, 0.81f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.46f, 0.46f, 0.46f, 0.95f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.73f, 0.60f, 0.15f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.87f, 0.87f, 0.87f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    }
}
