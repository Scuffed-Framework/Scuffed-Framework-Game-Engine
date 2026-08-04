#include "ImGuiDefaultWIDGETS.hpp"
#include <ImGui/ocornut/imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <Gui/Declare_Widget.hpp>

namespace SF::Engine
{

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