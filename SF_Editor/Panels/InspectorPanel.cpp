#include "InspectorPanel.hpp"
#include <Entity/Entity.hpp>
#include <Math/Transform.hpp>
#include <Gui/ocornut/imgui_internal.h>
#include <Gui/ocornut/imgui_stdlib.h>
#include <Gui/Declare_Widget.hpp>
#include "Panels.hpp"
#include <Default/ImGuiDefaultWIDGETS.hpp>
#include <Scene/SceneManager.hpp>

namespace SF::Engine
{
    void InspectorPanel::Draw()
    {
        EntityRegistry &registry =
            SceneManager::Get()->GetScene()->GetEntities()->GetRegistry();

        m_registry = &registry;

        ImGui::Begin("Inspector", &ShowInspector);

        // Validate entity
        if (m_entity && !registry.IsValid(m_entity))
        {
            m_entity = nullptr;
            m_entityId = 0;
        }

        if (!m_entity)
        {
            ImGui::TextDisabled("No entity selected");
            ImGui::End();
            return;
        }

        // Refresh entity if needed
        if (m_needsRefresh)
        {
            m_entity = registry.Find(m_entityId);
            m_needsRefresh = false;
        }

        if (!m_entity)
        {
            ImGui::TextDisabled("Entity not found");
            ImGui::End();
            return;
        }

        DrawEntityProperties();

        ImGui::Separator();
        ImGui::Spacing();

        DrawComponents();

        ImGui::Spacing();
        ImGui::Separator();

        DrawAddComponentMenu();

        ImGui::End();
    }

    void InspectorPanel::DrawEntityProperties()
    {
        ImGui::PushID("EntityProperties");
        // Name field
        ImGui::PushItemWidth(-1);
        std::string name = m_entity->GetName();
        if (ImGui::InputText("##Name", &name))
        {
            if (m_registry)
            {
                m_registry->RenameEntity(m_entity, name);
            }
            else
            {
                m_entity->SetName(name);
            }
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();

        // Active toggle
        bool active = m_entity->IsActive();
        if (ImGui::Checkbox("Active", &active))
        {
            m_entity->SetActive(active);
        }

        ImGui::PopID();
    }

    void InspectorPanel::DrawComponents()
    {
        if (!m_entity)
            return;

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Components");

        for (auto &[type, component] : m_entity->components)
        {
            if (!component)
                continue;

            ImGui::PushID(component.get());

            bool removed = false;

            if (ComponentWidgetRegistry::HasType(type))
            {
                ComponentWidgetRegistry::DrawByType(type, component.get(), "##w");

                // Right-click the header area for options, since the widget
                // owns its own header now and there's no "..." button on it.
                if (ImGui::BeginPopupContextItem("ComponentOptions"))
                {
                    if (ImGui::MenuItem("Remove"))
                    {
                        removed = m_entity->RemoveComponentByType(type);
                    }
                    ImGui::EndPopup();
                }
            }
            else
            {
                bool expanded = ImGui::CollapsingHeader(
                    component->GetTypeName().data(),
                    ImGuiTreeNodeFlags_DefaultOpen);

                ImGui::SameLine(ImGui::GetWindowWidth() - 30);
                if (ImGui::SmallButton("..."))
                    ImGui::OpenPopup("ComponentOptions");

                if (expanded)
                    DrawComponentField(std::string(component->GetTypeName()), component.get());

                if (ImGui::BeginPopup("ComponentOptions"))
                {
                    if (ImGui::MenuItem("Reset"))
                    {
                        component->Reset();
                    }
                    if (ImGui::MenuItem("Remove"))
                    {
                        removed = m_entity->RemoveComponentByType(type);
                    }
                    ImGui::EndPopup();
                }
            }

            ImGui::PopID();

            if (removed)
                break; // erased from the map we're range-for'ing over — stop iterating this frame
        }
    }

    void InspectorPanel::DrawAddComponentMenu()
    {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Add Component");

        static int selectedComponent = -1;
        const char *components[] = {
            "No",
            "No",
            "No",
            "No",
            "No",
            "No",
            "No"}; // when I add ComponentRegistry::Get()->GetAll()

        ImGui::PushItemWidth(-1);
        if (ImGui::Combo("##AddComponent", &selectedComponent, components, IM_ARRAYSIZE(components)))
        {
            // Handle component addition based on selection
            switch (selectedComponent)
            {
            case 0:
                // m_entity->AddComponent<idk>();
                break;
            case 1:
                // m_entity->AddComponent<dihh>();
                break;
            case 2:
                // m_entity->AddComponent<67>();
                break;
                // ... add other cases
            }
            selectedComponent = -1; // Reset selection
        }
        ImGui::PopItemWidth();
    }

    bool InspectorPanel::DrawComponentField(const std::string &label, SF::Engine::Component *component)
    {
        if (!component)
            return false;

        bool modified = false;

        // Example property drawing - adapt based on your component types
        ImGui::Indent();

        // Transform properties example
        if (component->GetTypeName() == "TransformComponent")
        {
            ImGui::Text("Position");
            ImGui::SameLine();
            static float pos[3] = {0.0f, 0.0f, 0.0f};
            if (ImGui::DragFloat3("##Position", pos, 0.1f))
            {
                modified = true;
            }

            ImGui::Text("Rotation");
            ImGui::SameLine();
            static float rot[3] = {0.0f, 0.0f, 0.0f};
            if (ImGui::DragFloat3("##Rotation", rot, 1.0f))
            {
                modified = true;
            }

            ImGui::Text("Scale");
            ImGui::SameLine();
            static float scale[3] = {1.0f, 1.0f, 1.0f};
            if (ImGui::DragFloat3("##Scale", scale, 0.1f, 0.01f, 100.0f))
            {
                modified = true;
            }
        }

        ImGui::Unindent();

        return modified;
    }

    void InspectorPanel::SetEntity(SF::Engine::Entity *entity)
    {
        m_entity = entity;
        if (entity)
        {
            m_entityId = entity->GetId();
        }
        else
        {
            m_entityId = 0;
        }
        m_needsRefresh = true;
    }

    void InspectorPanel::Refresh()
    {
        m_needsRefresh = true;
    }
}