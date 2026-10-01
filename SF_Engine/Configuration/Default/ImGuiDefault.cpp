#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGuiDefault.hpp"
#include <Gui/ImGui/ImGuizmoIncludes.hpp>

namespace SF::Engine
{
    void ImGuiDefaultStyle::SetStyle()
    {
        ImGui::StyleColorsDark();

        ImGuiStyle &style = ImGui::GetStyle();
        Vec4 *colors      = ImGui::GetStyle().Colors;

        colors[ImGuiCol_BorderShadow]                = Vec4(0.1f, 0.1f, 0.0f, 1.00);
        style.Colors[ImGuiCol_Text]                  = Vec4(1.00f, 1.00f, 1.00f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled]          = Vec4(0.50f, 0.50f, 0.50f, 1.00f);
        style.Colors[ImGuiCol_Border]                = Color(IM_COL32(61, 61, 61, 255)).AsVec4();
        style.Colors[ImGuiCol_BorderShadow]          = Vec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_FrameBg]               = Vec4(0.25f, 0.25f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_WindowBg]              = (Vec4) Color(IM_COL32(15, 15, 15, 255)).AsVec4();
        style.Colors[ImGuiCol_TitleBg]               = (Vec4) Color(IM_COL32(22, 22, 22, 255)).AsVec4();
        style.Colors[ImGuiCol_TitleBgActive]         = (Vec4) Color(IM_COL32(22, 22, 22, 255)).AsVec4();
        style.Colors[ImGuiCol_TitleBgCollapsed]      = (Vec4) Color(IM_COL32(22, 22, 22, 255)).AsVec4();
        style.Colors[ImGuiCol_FrameBg]               = Vec4(0.25f, 0.25f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered]        = Vec4(0.38f, 0.38f, 0.38f, 1.00f);
        style.Colors[ImGuiCol_FrameBgActive]         = Vec4(0.67f, 0.67f, 0.67f, 1.00f);
        style.Colors[ImGuiCol_MenuBarBg]             = Vec4(0.14f, 0.14f, 0.14f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarBg]           = Vec4(0.02f, 0.02f, 0.02f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrab]         = Vec4(0.31f, 0.31f, 0.31f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered]  = Vec4(0.41f, 0.41f, 0.41f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabActive]   = Vec4(0.51f, 0.51f, 0.51f, 1.00f);
        style.Colors[ImGuiCol_CheckMark]             = Vec4(0.11f, 0.64f, 0.92f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab]            = Vec4(0.11f, 0.64f, 0.92f, 1.00f);
        style.Colors[ImGuiCol_SliderGrabActive]      = Vec4(0.08f, 0.50f, 0.72f, 1.00f);
        style.Colors[ImGuiCol_Button]                = Vec4(0.25f, 0.25f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered]         = Vec4(0.38f, 0.38f, 0.38f, 1.00f);
        style.Colors[ImGuiCol_ButtonActive]          = Vec4(0.67f, 0.67f, 0.67f, 1.00f);
        style.Colors[ImGuiCol_Header]                = Vec4(0.22f, 0.22f, 0.22f, 1.00f);
        style.Colors[ImGuiCol_HeaderHovered]         = Vec4(0.25f, 0.25f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_HeaderActive]          = Vec4(0.67f, 0.67f, 0.67f, 1.00);
        style.Colors[ImGuiCol_Separator]             = style.Colors[ImGuiCol_Border];
        style.Colors[ImGuiCol_SeparatorHovered]      = Vec4(0.41f, 0.42f, 0.44f, 1.00f);
        style.Colors[ImGuiCol_SeparatorActive]       = Vec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_ResizeGrip]            = Vec4(0.00f, 0.00f, 0.00f, 0.00f);
        style.Colors[ImGuiCol_ResizeGripHovered]     = Vec4(0.29f, 0.30f, 0.31f, 1.00f);
        style.Colors[ImGuiCol_ResizeGripActive]      = Vec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_Tab]                   = Vec4(0.08f, 0.08f, 0.09f, 1.00f);
        style.Colors[ImGuiCol_TabHovered]            = Vec4(0.33f, 0.34f, 0.36f, 1.00f);
        style.Colors[ImGuiCol_DockingPreview]        = Vec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_DockingEmptyBg]        = Vec4(0.20f, 0.20f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_PlotLines]             = Vec4(0.61f, 0.61f, 0.61f, 1.00f);
        style.Colors[ImGuiCol_PlotLinesHovered]      = Vec4(1.00f, 0.43f, 0.35f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogram]         = Vec4(0.90f, 0.70f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_PlotHistogramHovered]  = Vec4(1.00f, 0.60f, 0.00f, 1.00f);
        style.Colors[ImGuiCol_TextSelectedBg]        = Vec4(0.26f, 0.59f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_DragDropTarget]        = Vec4(0.11f, 0.64f, 0.92f, 1.00f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = Vec4(1.00f, 1.00f, 1.00f, 1.00f);
        style.Colors[ImGuiCol_NavWindowingDimBg]     = Vec4(0.80f, 0.80f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_ModalWindowDimBg]      = Vec4(0.80f, 0.80f, 0.80f, 1.00f);
        style.Colors[ImGuiCol_CheckMark]             = Vec4(0.96f, 0.96f, 0.96f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab]            = Vec4(1.0f, 1.0f, 1.0f, 1.00f);
        style.Colors[ImGuiCol_SliderGrabActive]      = Vec4(1.0f, 1.0f, 1.0f, 1.00f);

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Vec2(6.0f, -1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, Vec2(2.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, Vec2(4.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, Vec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.0f);

        colors[ImGuiCol_BorderShadow] = Vec4(0.1f, 0.1f, 0.0f, 0.39f);
        style.WindowBorderSize        = 1;
        style.ChildBorderSize         = 1;
        style.PopupBorderSize         = 1;
        style.FrameBorderSize         = 1;
        style.TabBorderSize           = 1;
        style.WindowRounding          = 0;
        style.ChildRounding           = 0;
        style.FrameRounding           = 3;
        style.PopupRounding           = 0;
        style.ScrollbarRounding       = 0;
        style.GrabRounding            = 0;
        style.LogSliderDeadzone       = 0;
        style.TabRounding             = 0;

        ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;
        ImGui::GetIO().ConfigWindowsResizeFromEdges      = true;

        style.AntiAliasedLines         = true;
        style.WindowMenuButtonPosition = ImGuiDir_Right;
        style.PopupRounding            = 3;

        style.WindowPadding = Vec2(4, 4);
        style.FramePadding  = Vec2(6, 4);
        style.ItemSpacing   = Vec2(6, 2);

        style.ScrollbarSize = 18;

        style.WindowBorderSize = 1;
        style.ChildBorderSize  = 1;
        style.PopupBorderSize  = 1;
        style.FrameBorderSize  = 1;

        style.WindowRounding    = 3;
        style.ChildRounding     = 3;
        style.FrameRounding     = 0;
        style.ScrollbarRounding = 2;
        style.GrabRounding      = 0;

        style.TabBorderSize  = 0;
        style.TabRounding    = 3;
        style.WindowRounding = 0.0f;

        ImGuizmo::Style &styleGizmo           = ImGuizmo::GetStyle();
        styleGizmo.TranslationLineThickness   = 3.0f;
        styleGizmo.TranslationLineArrowSize   = 6.0f;
        styleGizmo.RotationLineThickness      = 4.0f;
        styleGizmo.RotationOuterLineThickness = 4.0f;
        styleGizmo.ScaleLineThickness         = 2.5f;
        styleGizmo.ScaleLineCircleSize        = 5.0f;
        styleGizmo.HatchedAxisLineThickness   = 6.0f;
        styleGizmo.CenterCircleSize           = 2.5f;

        styleGizmo.Colors[ImGuizmo::DIRECTION_X] = Vec4(1.0f, 0.21f, 0.23f, 0.9f);
        styleGizmo.Colors[ImGuizmo::DIRECTION_Y] = Vec4(0.60f, 0.9f, 0.067f, 0.9f);
        styleGizmo.Colors[ImGuizmo::DIRECTION_Z] = Vec4(0.184f, 0.218f, 0.98f, 0.9f);
        styleGizmo.Colors[ImGuizmo::PLANE_X]     = Vec4(0.99f, 0.2f, 0.23f, 0.6f);
        styleGizmo.Colors[ImGuizmo::PLANE_Y]     = Vec4(0.60f, 0.9f, 0.067f, 0.6f);
        styleGizmo.Colors[ImGuizmo::PLANE_Z]     = Vec4(0.184f, 0.218f, 0.98f, 0.6f);

        styleGizmo.Colors[ImGuizmo::SELECTION]             = Vec4(1.000f, 0.500f, 0.062f, 0.541f);
        styleGizmo.Colors[ImGuizmo::INACTIVE]              = Vec4(0.600f, 0.600f, 0.600f, 0.600f);
        styleGizmo.Colors[ImGuizmo::TRANSLATION_LINE]      = Vec4(0.666f, 0.666f, 0.666f, 0.666f);
        styleGizmo.Colors[ImGuizmo::SCALE_LINE]            = Vec4(0.250f, 0.250f, 0.250f, 1.000f);
        styleGizmo.Colors[ImGuizmo::ROTATION_USING_BORDER] = Vec4(1.000f, 0.500f, 0.062f, 1.000f);
        styleGizmo.Colors[ImGuizmo::ROTATION_USING_FILL]   = Vec4(1.000f, 0.500f, 0.062f, 0.500f);
        styleGizmo.Colors[ImGuizmo::HATCHED_AXIS_LINES]    = Vec4(0.000f, 0.000f, 0.000f, 0.500f);
        styleGizmo.Colors[ImGuizmo::TEXT]                  = Vec4(1.000f, 1.000f, 1.000f, 1.000f);
        styleGizmo.Colors[ImGuizmo::TEXT_SHADOW]           = Vec4(0.000f, 0.000f, 0.000f, 1.000f);

        ImGuizmo::AllowAxisFlip(false);

        colors[ImGuiCol_BorderShadow] = Vec4(0.1f, 0.1f, 0.0f, 0.39f);
        style.WindowBorderSize        = 1;
        style.ChildBorderSize         = 1;
        style.PopupBorderSize         = 1;
        style.FrameBorderSize         = 1;
        style.TabBorderSize           = 1;
        style.WindowRounding          = 0;
        style.ChildRounding           = 0;
        style.FrameRounding           = 1;
        style.PopupRounding           = 0;
        style.ScrollbarRounding       = 0;
        style.GrabRounding            = 2;
        style.GrabMinSize             = 8;
        style.LogSliderDeadzone       = 0;
        style.TabRounding             = 0;
        style.Alpha                   = 1.0f;
    }
    void ImGuiDefaultStyle::SetStyle2()
    {
        ImGuiStyle &style = ImGui::GetStyle();

        style.ColorButtonPosition = ImGuiDir_Right;

        style.Colors[ImGuiCol_Text]              = Vec4(1.00f, 1.00f, 1.00f, 1.00f);
        style.Colors[ImGuiCol_Border]            = Vec4(0.0, 0.54, 1.0f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip]        = Vec4(0.0, 0.54, 1.0f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripActive]  = Vec4(0.0, 0.64, 1.0f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripHovered] = Vec4(0.0, 0.58, 1.0f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]           = Vec4(0.10f, 0.10f, 0.10f, 0.1f);
        style.Colors[ImGuiCol_WindowBg]          = Vec4(0.00f, 0.00f, 0.00f, 0.5f);
        style.Colors[ImGuiCol_PopupBg]           = Vec4(0.00f, 0.00f, 0.00f, 0.5f);
        style.Colors[ImGuiCol_TitleBg]           = Vec4(0.05f, 0.05f, 0.05f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive]     = Vec4(0.05f, 0.05f, 0.05f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed]  = Vec4(0.05f, 0.05f, 0.05f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered]     = Vec4(0.00f, 1.00f, 0.00f, 0.50f);
        style.Colors[ImGuiCol_HeaderHovered]     = Vec4(0.00f, 1.00f, 0.00f, 0.50f);
        style.Colors[ImGuiCol_TabHovered]        = Vec4(0.00f, 1.00f, 0.00f, 0.50f);
        style.Colors[ImGuiCol_FrameBgHovered]    = Vec4(0.00f, 1.00f, 0.00f, 0.50f);
        style.Colors[ImGuiCol_Button]            = Vec4(0.10f, 0.10f, 0.10f, 0.50f);
        style.Colors[ImGuiCol_SliderGrab]        = Vec4(0.2f, 0.2f, 0.2f, 0.50f);
        style.Colors[ImGuiCol_FrameBgActive]     = Vec4(0.10f, 0.10f, 0.10f, 0.1f);
        style.Colors[ImGuiCol_Header]            = Vec4(0.25f, 0.25f, 0.25f, 0.5f);
        style.Colors[ImGuiCol_HeaderActive]      = Vec4(0.25f, 0.25f, 0.25f, 0.5f);
        style.Colors[ImGuiCol_HeaderHovered]     = Vec4(0.4f, 0.4f, 0.4f, 0.5f);

        style.FrameRounding    = 0;
        style.WindowBorderSize = 0.1f;
    }

    void ImGuiDefaultStyle::SetStyle3()
    {
        ImGuiStyle &style = ImGui::GetStyle();
        Vec4 *colors      = style.Colors;

        style.WindowRounding    = 0.0f;
        style.ChildRounding     = 0.0f;
        style.FrameRounding     = 0.0f;
        style.PopupRounding     = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding      = 0.0f;
        style.TabRounding       = 0.0f;

        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize  = 1.0f;
        style.PopupBorderSize  = 1.0f;
        style.FrameBorderSize  = 0.0f;
        style.TabBorderSize    = 0.0f;

        style.WindowPadding    = Vec2(8, 8);
        style.FramePadding     = Vec2(6, 4);
        style.ItemSpacing      = Vec2(6, 6);
        style.ItemInnerSpacing = Vec2(6, 4);
        style.IndentSpacing    = 14.0f;
        style.ScrollbarSize    = 12.0f;
        style.GrabMinSize      = 10.0f;

        style.ColorButtonPosition = ImGuiDir_Right;

        const Vec4 accent       = Vec4(0.00f, 0.54f, 1.00f, 1.00f); // signature blue
        const Vec4 accentHover  = Vec4(0.00f, 0.62f, 1.00f, 1.00f);
        const Vec4 accentActive = Vec4(0.00f, 0.68f, 1.00f, 1.00f);

        const Vec4 bgVoid        = Vec4(0.00f, 0.00f, 0.00f, 0.92f); // window/popup bg
        const Vec4 bgPanel       = Vec4(0.06f, 0.06f, 0.06f, 1.00f); // title bars
        const Vec4 bgField       = Vec4(0.11f, 0.11f, 0.11f, 1.00f); // frame bg
        const Vec4 bgFieldHover  = Vec4(0.16f, 0.16f, 0.16f, 1.00f);
        const Vec4 bgFieldActive = Vec4(0.14f, 0.14f, 0.14f, 1.00f);
        const Vec4 borderCol     = Vec4(0.20f, 0.20f, 0.20f, 1.00f);
        const Vec4 rowSelect     = Vec4(0.20f, 0.20f, 0.20f, 1.00f); // hierarchy selection grey
        const Vec4 rowHover      = Vec4(0.13f, 0.13f, 0.13f, 1.00f);
        const Vec4 textDim       = Vec4(0.55f, 0.55f, 0.55f, 1.00f);

        colors[ImGuiCol_Text]         = Vec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = textDim;
        colors[ImGuiCol_WindowBg]     = bgVoid;
        colors[ImGuiCol_ChildBg]      = Vec4(0, 0, 0, 0);
        colors[ImGuiCol_PopupBg]      = bgVoid;
        colors[ImGuiCol_Border]       = borderCol;
        colors[ImGuiCol_BorderShadow] = Vec4(0, 0, 0, 0);

        colors[ImGuiCol_FrameBg]        = bgField;
        colors[ImGuiCol_FrameBgHovered] = bgFieldHover;
        colors[ImGuiCol_FrameBgActive]  = bgFieldActive;

        colors[ImGuiCol_TitleBg]          = bgPanel;
        colors[ImGuiCol_TitleBgActive]    = bgPanel;
        colors[ImGuiCol_TitleBgCollapsed] = bgPanel;
        colors[ImGuiCol_MenuBarBg]        = bgPanel;

        colors[ImGuiCol_ScrollbarBg]          = Vec4(0, 0, 0, 0);
        colors[ImGuiCol_ScrollbarGrab]        = bgFieldHover;
        colors[ImGuiCol_ScrollbarGrabHovered] = Vec4(0.24f, 0.24f, 0.24f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive]  = Vec4(0.30f, 0.30f, 0.30f, 1.0f);

        colors[ImGuiCol_CheckMark]        = accent;
        colors[ImGuiCol_SliderGrab]       = accent;
        colors[ImGuiCol_SliderGrabActive] = accentActive;

        colors[ImGuiCol_Button]        = bgField;
        colors[ImGuiCol_ButtonHovered] = bgFieldHover;
        colors[ImGuiCol_ButtonActive]  = bgFieldActive;

        colors[ImGuiCol_Header]        = rowSelect;
        colors[ImGuiCol_HeaderHovered] = rowHover;
        colors[ImGuiCol_HeaderActive]  = rowSelect;

        colors[ImGuiCol_Separator]        = borderCol;
        colors[ImGuiCol_SeparatorHovered] = accent;
        colors[ImGuiCol_SeparatorActive]  = accentActive;

        colors[ImGuiCol_ResizeGrip]        = accent;
        colors[ImGuiCol_ResizeGripHovered] = accentHover;
        colors[ImGuiCol_ResizeGripActive]  = accentActive;

        colors[ImGuiCol_Tab]        = bgPanel;
        colors[ImGuiCol_TabHovered] = bgFieldHover;

        colors[ImGuiCol_PlotLines]            = accent;
        colors[ImGuiCol_PlotLinesHovered]     = accentHover;
        colors[ImGuiCol_PlotHistogram]        = accent;
        colors[ImGuiCol_PlotHistogramHovered] = accentHover;

        colors[ImGuiCol_TextSelectedBg]        = Vec4(accent.x, accent.y, accent.z, 0.35f);
        colors[ImGuiCol_DragDropTarget]        = accent;
        colors[ImGuiCol_NavWindowingHighlight] = Vec4(1.0f, 1.0f, 1.0f, 0.7f);
        colors[ImGuiCol_NavWindowingDimBg]     = Vec4(0.0f, 0.0f, 0.0f, 0.5f);
        colors[ImGuiCol_ModalWindowDimBg]      = Vec4(0.0f, 0.0f, 0.0f, 0.5f);
    }

    void ImGuiDefaultStyle::SetStyle4()
    {
        Vec4 *colors                           = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text]                  = Vec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]          = Vec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]              = Vec4(0.06f, 0.06f, 0.06f, 0.94f);
        colors[ImGuiCol_ChildBg]               = Vec4(1.00f, 1.00f, 1.00f, 0.00f);
        colors[ImGuiCol_PopupBg]               = Vec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border]                = Vec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow]          = Vec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]               = Vec4(0.20f, 0.21f, 0.22f, 0.54f);
        colors[ImGuiCol_FrameBgHovered]        = Vec4(0.40f, 0.40f, 0.40f, 0.40f);
        colors[ImGuiCol_FrameBgActive]         = Vec4(0.18f, 0.18f, 0.18f, 0.67f);
        colors[ImGuiCol_TitleBg]               = Vec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive]         = Vec4(0.29f, 0.29f, 0.29f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]      = Vec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]             = Vec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]           = Vec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]         = Vec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]  = Vec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]   = Vec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]             = Vec4(0.94f, 0.94f, 0.94f, 1.00f);
        colors[ImGuiCol_SliderGrab]            = Vec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]      = Vec4(0.86f, 0.86f, 0.86f, 1.00f);
        colors[ImGuiCol_Button]                = Vec4(0.44f, 0.44f, 0.44f, 0.40f);
        colors[ImGuiCol_ButtonHovered]         = Vec4(0.46f, 0.47f, 0.48f, 1.00f);
        colors[ImGuiCol_ButtonActive]          = Vec4(0.42f, 0.42f, 0.42f, 1.00f);
        colors[ImGuiCol_Header]                = Vec4(0.70f, 0.70f, 0.70f, 0.31f);
        colors[ImGuiCol_HeaderHovered]         = Vec4(0.70f, 0.70f, 0.70f, 0.80f);
        colors[ImGuiCol_HeaderActive]          = Vec4(0.48f, 0.50f, 0.52f, 1.00f);
        colors[ImGuiCol_Separator]             = Vec4(0.43f, 0.43f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]      = Vec4(0.72f, 0.72f, 0.72f, 0.78f);
        colors[ImGuiCol_SeparatorActive]       = Vec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_ResizeGrip]            = Vec4(0.91f, 0.91f, 0.91f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered]     = Vec4(0.81f, 0.81f, 0.81f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]      = Vec4(0.46f, 0.46f, 0.46f, 0.95f);
        colors[ImGuiCol_PlotLines]             = Vec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]      = Vec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]         = Vec4(0.73f, 0.60f, 0.15f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]  = Vec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]        = Vec4(0.87f, 0.87f, 0.87f, 0.35f);
        colors[ImGuiCol_DragDropTarget]        = Vec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavWindowingHighlight] = Vec4(1.00f, 1.00f, 1.00f, 0.70f);
    }
} // namespace SF::Engine
