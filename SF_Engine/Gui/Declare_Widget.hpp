#pragma once

/**
 * Declarative macro DSL for building engine inspector widgets.
 *
 * **USAGE**
 *
 *  // 1. Declare + auto-register a widget for MyComponent:
 *
 *  SF_COMPONENT_WIDGET(MyComponent, "My Component")
 *  {
 *      SF_WIDGET_VEC3  ("Position", comp.position,     0,0,0, 0.05f)
 *      SF_WIDGET_VEC3  ("Rotation", comp.rotation,     0,0,0, 0.5f )
 *      SF_WIDGET_FLOAT ("Speed",    comp.speed,        0, 100, "%.2f")
 *      SF_WIDGET_BOOL  ("Active",   comp.active)
 *      SF_WIDGET_COLOR ("Tint",     comp.color)
 *      SF_WIDGET_RESET (comp.Reset())
 *  }
 *
 *  // 2. Draw any registered component from an inspector:
 *
 *  ComponentWidgetRegistry::Draw(myComp, "##myComp");
 *
 *  SF_WIDGET_VEC3   (label, vec3ref, rx, ry, rz, speed)
 *  SF_WIDGET_FLOAT  (label, ref, vmin, vmax, fmt)
 *  SF_WIDGET_FLOAT_DRAG (label, ref, speed, vmin, vmax, fmt)
 *  SF_WIDGET_INT    (label, ref, vmin, vmax)
 *  SF_WIDGET_BOOL   (label, ref)
 *  SF_WIDGET_COLOR  (label, vec4ref)               : RGBA colour picker
 *  SF_WIDGET_COLOR3 (label, vec3ref)               : RGB colour picker
 *  SF_WIDGET_COMBO  (label, intref, items_cstr)    : null-delimited item list
 *  SF_WIDGET_TEXT   (label, cstr)                  : read-only display
 *  SF_WIDGET_SWATCH (vec4ref)                      : full-width colour bar
 *  SF_WIDGET_BAR    (value, maxValue, vec3color)   : intensity progress bar
 *  SF_WIDGET_SECTION(label)                        : sub-section separator
 *  SF_WIDGET_RESET  (expr)                         : right-aligned Reset button
 *
 */

#include <Gui/ocornut/imgui.h>
#include <Math/BasicMath.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <functional>
#include <typeindex>
#include <unordered_map>

namespace SF::Engine
{

    namespace WidgetTheme
    {
        inline constexpr ImVec4 kAccent = {0.00f, 0.54f, 1.00f, 1.00f};
        inline constexpr ImVec4 kAccentDim = {0.00f, 0.54f, 1.00f, 1.00f};
        inline constexpr ImVec4 kRed = {0.86f, 0.20f, 0.20f, 1.00f};
        inline constexpr ImVec4 kRedDim = {0.86f, 0.20f, 0.20f, 1.00f};
        inline constexpr ImVec4 kGreen = {0.20f, 0.80f, 0.20f, 1.00f};
        inline constexpr ImVec4 kGreenDim = {0.20f, 0.80f, 0.20f, 1.00f};
        inline constexpr ImVec4 kBlue = {0.20f, 0.40f, 1.00f, 1.00f};
        inline constexpr ImVec4 kBlueDim = {0.20f, 0.40f, 1.00f, 1.00f};
        inline constexpr ImVec4 kComponentBg = {0.08f, 0.08f, 0.08f, 1.00f};
        inline constexpr ImVec4 kHeaderBg = {0.12f, 0.12f, 0.14f, 1.00f};
    } // namespace WidgetTheme

    class WidgetBuilder
    {
    public:
        /**
         * @param sectionLabel   Shown in the CollapsingHeader.
         * @param uniqueId       PushID string (pass the caller-supplied id).
         * @param labelColWidth  Width in pixels of the left (label) column.
         */
        explicit WidgetBuilder(const char *sectionLabel,
                               const char *uniqueId,
                               float labelColWidth = 90.0f)
            : m_sectionLabel(sectionLabel)
        {
            ImGui::PushID(uniqueId);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, WidgetTheme::kComponentBg);
            ImGui::BeginChild(m_childId, ImVec2(0, 0), ImGuiChildFlags_AutoResizeY);

            m_open = DrawSectionHeader(sectionLabel);

            if (m_open)
            {
                ImGui::Spacing();
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 3));
                m_tableOpen = ImGui::BeginTable(m_tableId,
                                                2,
                                                ImGuiTableFlags_SizingStretchProp);
                if (m_tableOpen)
                {
                    ImGui::TableSetupColumn("Label",
                                            ImGuiTableColumnFlags_WidthFixed,
                                            labelColWidth);
                    ImGui::TableSetupColumn("Value",
                                            ImGuiTableColumnFlags_WidthStretch);
                }
            }
        }

        ~WidgetBuilder()
        {
            if (m_open)
            {
                if (m_tableOpen)
                {
                    ImGui::EndTable();
                    ImGui::PopStyleVar(); // CellPadding
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(); // ChildBg
            ImGui::PopID();
        }

        // Returns true if the section is expanded (safe to add rows).
        bool IsOpen() const { return m_open && m_tableOpen; }

        // Call after EndTable to draw a right-aligned small Reset button.
        void DrawResetButton(const char *label = "Reset") const
        {
            if (!m_open)
                return;
            char lbl[64];
            std::snprintf(lbl, sizeof(lbl), "%s##Rst_%s", label, m_sectionLabel);
            ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 52.0f + ImGui::GetCursorPosX());
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
            ImGui::SmallButton(lbl);
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        void PropLabel(const char *label) const
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", label);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
        }

        void Vec3Row(const char *id, float *v,
                     float rx, float ry, float rz,
                     float speed = 0.1f,
                     const char *fmt = "%.3f") const
        {
            const float btnW = ImGui::GetFrameHeight();

            auto xyzButton = [&](const char *axis, ImVec4 dimCol, ImVec4 hovCol,
                                 float &component, float resetVal)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, dimCol);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovCol);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, hovCol);
                char bid[32];
                std::snprintf(bid, sizeof(bid), "%s##%s", axis, id);
                if (ImGui::Button(bid, ImVec2(btnW, 0)))
                    component = resetVal;
                ImGui::PopStyleColor(3);
            };

            // X
            xyzButton("X", WidgetTheme::kRedDim, WidgetTheme::kRed, v[0], rx);
            ImGui::SameLine(0, 1);
            ImGui::PushItemWidth((ImGui::GetContentRegionAvail().x - btnW * 2 - 4.0f) / 3.0f);
            {
                char did[32];
                std::snprintf(did, sizeof(did), "##vx_%s", id);
                ImGui::DragFloat(did, &v[0], speed, 0, 0, fmt);
            }
            ImGui::PopItemWidth();

            // Y
            ImGui::SameLine(0, 4);
            xyzButton("Y", WidgetTheme::kGreenDim, WidgetTheme::kGreen, v[1], ry);
            ImGui::SameLine(0, 1);
            ImGui::PushItemWidth((ImGui::GetContentRegionAvail().x - btnW - 4.0f) / 2.0f);
            {
                char did[32];
                std::snprintf(did, sizeof(did), "##vy_%s", id);
                ImGui::DragFloat(did, &v[1], speed, 0, 0, fmt);
            }
            ImGui::PopItemWidth();

            // Z
            ImGui::SameLine(0, 4);
            xyzButton("Z", WidgetTheme::kBlueDim, WidgetTheme::kBlue, v[2], rz);
            ImGui::SameLine(0, 1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            {
                char did[32];
                std::snprintf(did, sizeof(did), "##vz_%s", id);
                ImGui::DragFloat(did, &v[2], speed, 0, 0, fmt);
            }
        }

        void ColorSwatch(const Vec4 &c) const
        {
            ImVec2 sz{ImGui::GetContentRegionAvail().x, 8.0f};
            ImVec2 cp = ImGui::GetCursorScreenPos();
            ImU32 col = IM_COL32(int(c.r * 255), int(c.g * 255),
                                 int(c.b * 255), 255);
            ImGui::GetWindowDrawList()->AddRectFilled(
                cp, ImVec2(cp.x + sz.x, cp.y + sz.y), col);
            ImGui::Dummy(sz);
            ImGui::Spacing();
        }

        // Draws a full-width intensity bar (e.g. light intensity preview).
        void IntensityBar(float value, float maxValue, const Vec3 &colour) const
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            float barW = ImGui::GetContentRegionAvail().x;
            float fillW = barW * glm::clamp(value / maxValue, 0.0f, 1.0f);
            ImGui::GetWindowDrawList()->AddRectFilled(
                cp, ImVec2(cp.x + barW, cp.y + 4),
                IM_COL32(30, 30, 30, 255), 2);
            ImGui::GetWindowDrawList()->AddRectFilled(
                cp, ImVec2(cp.x + fillW, cp.y + 4),
                IM_COL32(int(colour.r * 220),
                         int(colour.g * 220),
                         int(colour.b * 220), 200),
                2);
            ImGui::Dummy(ImVec2(barW, 4));
            ImGui::Spacing();
        }

        // Sub-section visual separator (no collapsing, just a labelled divider).
        void SubSection(const char *label) const
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, WidgetTheme::kAccent);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::TableSetColumnIndex(1);
            float y = ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight() * 0.5f;
            ImGui::GetWindowDrawList()->AddLine(
                ImVec2(ImGui::GetCursorScreenPos().x, y),
                ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x, y),
                IM_COL32(255, 255, 255, 30), 1.0f);
            ImGui::Spacing();
        }

    private:
        static bool DrawSectionHeader(const char *label)
        {
            ImGui::PushStyleColor(ImGuiCol_Header,
                                  WidgetTheme::kHeaderBg);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                  ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                  ImVec4(0.22f, 0.22f, 0.28f, 1.0f));

            ImVec2 p = ImGui::GetCursorScreenPos();
            float h = ImGui::GetFrameHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p, ImVec2(p.x + 3.0f, p.y + h),
                IM_COL32(0, 138, 255, 200));
            ImGui::SetCursorScreenPos(ImVec2(p.x + 6.0f, p.y));

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
            bool open = ImGui::CollapsingHeader(label);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            return open;
        }

        const char *m_sectionLabel;
        char m_childId[64] = "##WB_Child";
        char m_tableId[64] = "##WB_Table";
        bool m_open = false;
        bool m_tableOpen = false;
    };

//
/// Vec3 with X/Y/Z reset buttons.
/// @param label  Display name
/// @param ref    Vec3 l-value
/// @param rx/ry/rz  Per-axis reset values
/// @param speed  Drag speed (default 0.1)
#define SF_WIDGET_VEC3(label, ref, rx, ry, rz, speed) \
    if (_wb.IsOpen())                                 \
    {                                                 \
        _wb.PropLabel(label);                         \
        _wb.Vec3Row(label, glm::value_ptr(ref),       \
                    (rx), (ry), (rz), (speed));       \
    }

/// Vec3 with degree format (convenience wrapper over SF_WIDGET_VEC3).
#define SF_WIDGET_VEC3_ANGLES(label, ref, speed) \
    SF_WIDGET_VEC3(label, ref, 0.0f, 0.0f, 0.0f, speed)

/// Slider for a float in [vmin, vmax].
#define SF_WIDGET_FLOAT(label, ref, vmin, vmax, fmt)           \
    if (_wb.IsOpen())                                          \
    {                                                          \
        _wb.PropLabel(label);                                  \
        ImGui::SliderFloat("##" label, &(ref),                 \
                           (float)(vmin), (float)(vmax), fmt); \
    }

/// DragFloat (unclamped or clamped) for a float property.
#define SF_WIDGET_FLOAT_DRAG(label, ref, speed, vmin, vmax, fmt) \
    if (_wb.IsOpen())                                            \
    {                                                            \
        _wb.PropLabel(label);                                    \
        ImGui::DragFloat("##" label, &(ref),                     \
                         (float)(speed),                         \
                         (float)(vmin), (float)(vmax), fmt);     \
    }

/// SliderInt for an int property in [vmin, vmax].
#define SF_WIDGET_INT(label, ref, vmin, vmax)                 \
    if (_wb.IsOpen())                                         \
    {                                                         \
        _wb.PropLabel(label);                                 \
        ImGui::SliderInt("##" label, &(ref), (vmin), (vmax)); \
    }

/// Checkbox for a bool property.
#define SF_WIDGET_BOOL(label, ref)           \
    if (_wb.IsOpen())                        \
    {                                        \
        _wb.PropLabel(label);                \
        ImGui::Checkbox("##" label, &(ref)); \
    }

/// RGBA colour picker (swatch + picker).
/// @param ref  Vec4 l-value
#define SF_WIDGET_COLOR(label, ref)                              \
    if (_wb.IsOpen())                                            \
    {                                                            \
        _wb.PropLabel(label);                                    \
        ImGui::ColorEdit4("##" label, glm::value_ptr(ref),       \
                          ImGuiColorEditFlags_NoInputs |         \
                              ImGuiColorEditFlags_Float |        \
                              ImGuiColorEditFlags_PickerHueBar); \
    }

/// RGB colour picker (swatch + picker, no alpha).
/// @param ref  Vec3 l-value
#define SF_WIDGET_COLOR3(label, ref)                             \
    if (_wb.IsOpen())                                            \
    {                                                            \
        _wb.PropLabel(label);                                    \
        ImGui::ColorEdit3("##" label, glm::value_ptr(ref),       \
                          ImGuiColorEditFlags_NoInputs |         \
                              ImGuiColorEditFlags_Float |        \
                              ImGuiColorEditFlags_PickerHueBar); \
    }

/// Combo box driven by a null-delimited C string.
/// @param ref       int l-value (current index)
/// @param items     Null-delimited items, e.g. "Point\0Spot\0Directional\0"
#define SF_WIDGET_COMBO(label, ref, items)       \
    if (_wb.IsOpen())                            \
    {                                            \
        _wb.PropLabel(label);                    \
        ImGui::Combo("##" label, &(ref), items); \
    }

/// Read-only text row.
#define SF_WIDGET_TEXT(label, cstr)      \
    if (_wb.IsOpen())                    \
    {                                    \
        _wb.PropLabel(label);            \
        ImGui::TextDisabled("%s", cstr); \
    }

/// Typically used below a colour property for a preview.
/// @param vec4ref  const Vec4&
#define SF_WIDGET_SWATCH(vec4ref)     \
    if (_wb.IsOpen())                 \
    {                                 \
        if (_wb_tableWasOpen)         \
        {                             \
            ImGui::EndTable();        \
            ImGui::PopStyleVar();     \
            _wb_tableWasOpen = false; \
        }                             \
        _wb.ColorSwatch(vec4ref);     \
    }

/// Full-width intensity bar (use outside the table, e.g. light preview).
/// @param value      Current value
/// @param maxValue   Scale maximum
/// @param vec3color  Vec3 bar colour
#define SF_WIDGET_BAR(value, maxValue, vec3color)           \
    if (_wb.IsOpen())                                       \
    {                                                       \
        _wb.IntensityBar((value), (maxValue), (vec3color)); \
    }

/// Labelled sub-section divider inside the table.
#define SF_WIDGET_SECTION(label) \
    if (_wb.IsOpen())            \
    {                            \
        _wb.SubSection(label);   \
    }

/// Right-aligned Reset button. expr is evaluated when clicked.
#define SF_WIDGET_RESET(expr)                                                                    \
    if (_wb.IsOpen())                                                                            \
    {                                                                                            \
        if (ImGui::IsItemDeactivated())                                                          \
        {                                                                                        \
        }                                                                                        \
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 52.0f + ImGui::GetCursorPosX()); \
        ImGui::PushStyleColor(ImGuiCol_Button,                                                   \
                              ImVec4(0.2f, 0.2f, 0.2f, 1.0f));                                   \
        if (ImGui::SmallButton("Reset##_wb_rst"))                                                \
        {                                                                                        \
            expr;                                                                                \
        }                                                                                        \
        ImGui::PopStyleColor();                                                                  \
        ImGui::Spacing();                                                                        \
    }

    /**
     * Forward declaration of the auto-registration helper.
     * Defined just below SF_COMPONENT_WIDGET.
     */
    class ComponentWidgetRegistry;

#define SF_COMPONENT_WIDGET(ComponentType, SectionLabel)                       \
    /* Free function: DrawWidget_##ComponentType(void* ptr, const char* id) */ \
    static void DrawWidget_##ComponentType(void *_ptr, const char *_id)        \
    {                                                                          \
        auto &comp = *static_cast<ComponentType *>(_ptr);                      \
        WidgetBuilder _wb(SectionLabel, _id);                                  \
        (void)comp;                                                            \
        (void)_wb; /* suppress unused-variable warnings */                     \
                   /*  user body begins  */

/* Close the body and install the registrar. */
#define SF_COMPONENT_WIDGET_END(ComponentType)                           \
    /*  user body ends  */                                               \
    }                                                                    \
    /* Static registrar: runs before main, inserts into the registry. */ \
    namespace                                                            \
    {                                                                    \
        struct _WidgetReg_##ComponentType                                \
        {                                                                \
            _WidgetReg_##ComponentType()                                 \
            {                                                            \
                ComponentWidgetRegistry::Register<ComponentType>(        \
                    DrawWidget_##ComponentType);                         \
            }                                                            \
        } _gWidgetReg_##ComponentType;                                   \
    }
    class ComponentWidgetRegistry
    {
    public:
        using DrawFn = void (*)(void *compPtr, const char *id);

        template <typename T>
        static void Register(DrawFn fn)
        {
            Get().m_drawFns[std::type_index(typeid(T))] = fn;
        }

        /**
         * Draw any registered component.
         * Returns false if no widget is registered for that type.
         */
        template <typename T>
        static bool Draw(T &comp, const char *id)
        {
            auto &reg = Get();
            auto it = reg.m_drawFns.find(std::type_index(typeid(T)));
            if (it == reg.m_drawFns.end())
                return false;
            it->second(static_cast<void *>(&comp), id);
            return true;
        }

        /** Returns true if a widget is registered for type T. */
        template <typename T>
        static bool Has()
        {
            return Get().m_drawFns.count(std::type_index(typeid(T))) > 0;
        }

    private:
        ComponentWidgetRegistry() = default;
        static ComponentWidgetRegistry &Get()
        {
            static ComponentWidgetRegistry inst;
            return inst;
        }
        std::unordered_map<std::type_index, DrawFn> m_drawFns;
    };

    //  Convenience: SF_DECLARE_WIDGET
    //
    // Shorter combined macro for widgets that don't need SF_WIDGET_SWATCH
    // (i.e. no post-table drawing). Keeps the open/close in one declaration.
    //
    // Usage (must be in a .cpp file, not a header, to avoid ODR violations):
    //
    //   SF_DECLARE_WIDGET(TransformComponent, "Transform", 70.0f)
    //   {
    //       SF_WIDGET_VEC3("Position", comp.position, 0, 0, 0, 0.05f)
    //       SF_WIDGET_VEC3("Rotation", comp.rotation, 0, 0, 0, 0.50f)
    //       SF_WIDGET_VEC3("Scale",    comp.scale,    1, 1, 1, 0.01f)
    //       SF_WIDGET_RESET(comp.Reset())
    //   }
    //

#define SF_DECLARE_WIDGET(ComponentType, SectionLabel, LabelColWidth)   \
    static void DrawWidget_##ComponentType(void *_ptr, const char *_id) \
    {                                                                   \
        auto &comp = *static_cast<ComponentType *>(_ptr);               \
        WidgetBuilder _wb(SectionLabel, _id, LabelColWidth);            \
        (void)comp;                                                     \
        (void)_wb;

#define SF_DECLARE_WIDGET_END(ComponentType)                      \
    }                                                             \
    namespace                                                     \
    {                                                             \
        struct _WidgetReg_##ComponentType                         \
        {                                                         \
            _WidgetReg_##ComponentType()                          \
            {                                                     \
                ComponentWidgetRegistry::Register<ComponentType>( \
                    DrawWidget_##ComponentType);                  \
            }                                                     \
        } _gWidgetReg_##ComponentType;                            \
    }

} // namespace SF::Engine