#pragma once

#include <ImGui/ocornut/imgui.h>
#include <Default/TransformComponent.hpp>
#include <Graphics/Lighting/Light.hpp>
#include <Graphics/Lighting/LitMeshPipelinePass.hpp> // MeshMaterial

namespace SF::Engine
{
    //
    // Shared helpers : used across all widgets
    //

    // Unity-style section header: coloured left-bar + bold label
    // Returns true if the section body should be drawn (same as CollapsingHeader).
    bool SectionHeader(const char *label, bool *visible = nullptr);

    // Draw a row label in the left column and position the cursor in the
    // right column, ready for a widget.  Call inside a table.
    void PropLabel(const char *label);

    //
    // Transform widget
    //
    class TransformWidget
    {
    public:
        /**
         * @brief Draw the Transform component panel.
         * @param transform  Reference to the transform being edited : modified in-place.
         * @param id         Unique ImGui ID string (use entity name / index).
         */
        static void Draw(TransformComponent &transform, const char *id = "##Transform");
    };

    //
    // Material widget
    //
    class MaterialWidget
    {
    public:
        /**
         * @brief Draw the Material component panel (Unity "Mesh Renderer" style).
         * @param material  Reference to the PBR material being edited.
         * @param id        Unique ImGui ID string.
         */
        static void Draw(MeshMaterial &material, const char *id = "##Material");
    };

    //
    // Light widget
    //
    class LightWidget
    {
    public:
        /**
         * @brief Draw the Light component panel (type + colour + intensity + range).
         * @param light  Reference to the light being edited.
         * @param id     Unique ImGui ID string.
         */
        static void Draw(Light &light, const char *id = "##Light");
    };

    //
    // Backwards-compat wrapper (old static API still compiles)
    //
    class ImGuiWidgets
    {
    public:
        // Old signature : draws with internal static state (no external data).
        static void TransformsWidget();
    };
}
