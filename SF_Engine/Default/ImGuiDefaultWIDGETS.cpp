#include "ImGuiDefaultWIDGETS.hpp"
#include <ImGui/ocornut/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <Gui/Declare_Widget.hpp>

namespace SF::Engine
{
     SF_DECLARE_WIDGET(Transform, "Transform", 70.0f){
        SF_WIDGET_VEC3("Position", comp.position, 0, 0, 0, 0.05f)
            SF_WIDGET_VEC3("Rotation", comp.rotation, 0, 0, 0, 0.5f)
                SF_WIDGET_VEC3("Scale", comp.scale, 1, 1, 1, 0.01f)
                    SF_WIDGET_RESET(comp.Reset())} SF_DECLARE_WIDGET_END(Transform)

        SF_DECLARE_WIDGET(MeshMaterial, "Material", 90.0f){
            SF_WIDGET_COLOR("Albedo", comp.baseColor)
                SF_WIDGET_FLOAT("Metallic", comp.metallicFactor, 0.0f, 1.0f, "%.2f")
                    SF_WIDGET_FLOAT("Roughness", comp.roughnessFactor, 0.0f, 1.0f, "%.2f")
                        SF_WIDGET_FLOAT("AO", comp.aoFactor, 0.0f, 1.0f, "%.2f")
                            SF_WIDGET_FLOAT("Emission", comp.emissiveFactor, 0.0f, 4.0f, "%.2f")} SF_DECLARE_WIDGET_END(MeshMaterial)

            SF_DECLARE_WIDGET(Light, "Light", 90.0f)
    {
        {
            int typeIdx = static_cast<int>(comp.type);
            SF_WIDGET_COMBO("Type", typeIdx, "Point\0Spot\0Directional\0")
            comp.type = static_cast<Lighting::LightType>(typeIdx);
        }

        SF_WIDGET_COLOR3("Color", comp.color)
        SF_WIDGET_FLOAT_DRAG("Intensity", comp.intensity, 0.1f, 0.0f, 500.0f, "%.2f")

        if (comp.type != Lighting::LightType::Directional)
        {
            SF_WIDGET_FLOAT_DRAG("Range", comp.radius, 0.1f, 0.1f, 500.0f, "%.2f")
        }

        if (comp.type == Lighting::LightType::Spot)
        {
            SF_WIDGET_SECTION("Cone")
            SF_WIDGET_FLOAT("Inner Angle", comp.innerConeAngleDeg,
                            0.0f, comp.outerConeAngleDeg, "%.1f deg")
            SF_WIDGET_FLOAT("Outer Angle", comp.outerConeAngleDeg,
                            comp.innerConeAngleDeg, 180.0f, "%.1f deg")
        }

        SF_WIDGET_BOOL("Cast Shadow", comp.castShadow)
        // Intensity bar drawn outside via legacy wrapper below
    }
    
    SF_DECLARE_WIDGET_END(Light)

    void TransformWidget::Draw(Transform &t, const char *id)
    {
        ComponentWidgetRegistry::Draw(t, id);
    }

    void MaterialWidget::Draw(MeshMaterial &mat, const char *id)
    {
        ComponentWidgetRegistry::Draw(mat, id);

        // Append swatch below (outside the child window; it was already closed).
        // If restructured, call _wb.ColorSwatch(mat.baseColor) at end of block.
    }

    void LightWidget::Draw(Light &light, const char *id)
    {
        ComponentWidgetRegistry::Draw(light, id);
        // The intensity bar is drawn separately here for legacy compat.
        // In a future cleanup, use SF_WIDGET_BAR inside the DSL block.
        //   SF_WIDGET_BAR(light.intensity, 50.0f, light.color)
    }
} // namespace SF::Engine