#include "HierarchyPanel.hpp"
#include <Entity/Entity.hpp>
#include <Gui/ocornut/imgui_internal.h>
#include "Panels.hpp"
#include <Scene/SceneManager.hpp>

namespace SF::Engine
{
    // Todo: add folders, they are an entity with tag: "Folder"
    void HierarchyPanel::Draw()
    {
        EntityRegistry &registry = SceneManager::Get()->GetScene()->GetEntities()->GetRegistry();
        Scene *scene = SceneManager::Get()->GetScene();
        ImGui::Begin("Hierarchy", &ShowHierarchy);

        // Search filter
        static char searchBuffer[256] = "";
        ImGui::PushItemWidth(-1);
        if (ImGui::InputTextWithHint("##search", "Search...", searchBuffer, sizeof(searchBuffer)))
        {
            m_needsRefresh = true;
        }
        ImGui::PopItemWidth();

        ImGui::Separator();

        // Calculate visible entities for row backgrounds
        std::vector<SF::Engine::Entity *> visibleEntities;
        std::string searchStr(searchBuffer);

        for (auto &root : registry.GetRoots())
        {
            CollectVisibleEntities(root.get(), visibleEntities);
        }

        // Filter entities based on search
        if (!searchStr.empty())
        {
            visibleEntities.erase(
                std::remove_if(visibleEntities.begin(), visibleEntities.end(),
                               [&searchStr](SF::Engine::Entity *e)
                               {
                                   return e->GetName().find(searchStr) == std::string::npos;
                               }),
                visibleEntities.end());
        }

        // Draw alternating row backgrounds
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        float rowHeight = ImGui::GetTextLineHeightWithSpacing();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
        ImVec2 windowPos = ImGui::GetWindowPos();

        ImU32 colEven = ImGui::GetColorU32(ImGuiCol_TableRowBg, 0.3f);
        ImU32 colOdd = ImGui::GetColorU32(ImGuiCol_TableRowBgAlt, 0.3f);

        for (size_t i = 0; i < visibleEntities.size(); i++)
        {
            ImVec2 rowMin = ImVec2(
                windowPos.x + contentMin.x,
                ImGui::GetCursorScreenPos().y + i * rowHeight);
            ImVec2 rowMax = ImVec2(
                windowPos.x + contentMax.x,
                rowMin.y + rowHeight);

            drawList->AddRectFilled(rowMin, rowMax, (i % 2 == 0) ? colEven : colOdd);
        }

        // Draw entity nodes
        ImGui::BeginChild("HierarchyTree", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground);

        for (auto &root : registry.GetRoots())
        {
            DrawEntityNode(root.get());
        }

        // Handle right-click context menu
        if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawCreateOptions();
            ImGui::EndPopup();
        }

        ImGui::EndChild();
        ImGui::End();
    }

    void HierarchyPanel::DrawEntityNode(SF::Engine::Entity *entity)
    {
        if (!entity)
            return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanFullWidth |
                                   ImGuiTreeNodeFlags_OpenOnDoubleClick;

        // Handle leaf nodes
        if (entity->GetChildren().empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        // Selection highlighting
        if (entity->GetId() == m_selectedId)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // Use entity ID as unique identifier
        ImGui::PushID(static_cast<int>(entity->GetId()));

        // Entity active state indicator
        ImGui::PushStyleColor(ImGuiCol_Text, entity->IsActive() ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        bool opened = ImGui::TreeNodeEx("##EntityNode", flags, "%s", entity->GetName().c_str());

        ImGui::PopStyleColor();

        // Handle selection
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
        {
            m_selectedId = entity->GetId();
            m_selectedEntity = entity;
            if (m_onEntitySelected)
            {
                m_onEntitySelected(entity);
            }
        }

        // Drag and drop source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            const EntityId entityId = entity->GetId();

            ImGui::SetDragDropPayload(
                "ENTITY",
                &entityId,
                sizeof(EntityId));
            ImGui::Text("%s", entity->GetName().c_str());
            ImGui::EndDragDropSource();
        }

        // Drag and drop target (for reparenting)
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY"))
            {
                EntityId draggedId = *static_cast<const EntityId *>(payload->Data);
                // Handle reparenting in your registry
                // registry.Reparent(draggedEntity, entity);
            }
            ImGui::EndDragDropTarget();
        }

        // Context menu
        if (ImGui::BeginPopupContextItem("EntityContext"))
        {
            if (ImGui::MenuItem("Create Child"))
            {
                // Create child entity
                m_needsRefresh = true;
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Duplicate"))
            {
                // Duplicate entity
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Del"))
            {
                entity->MarkForRemoval();
                if (entity->GetId() == m_selectedId)
                {
                    m_selectedId = 0;
                    m_selectedEntity = nullptr;
                }
                m_needsRefresh = true;
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        if (opened)
        {
            for (auto &child : entity->GetChildren())
            {
                DrawEntityNode(child.get());
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    void HierarchyPanel::DrawRowBackground(float height)
    {
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        ImRect rowRect(
            window->WorkRect.Min.x,
            window->DC.CursorPos.y - height,
            window->WorkRect.Max.x,
            window->DC.CursorPos.y);

        static int rowCount = 0;
        ImU32 bgColor = (rowCount++ % 2 == 0)
                            ? ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.18f, 1.0f))
                            : ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.22f, 1.0f));

        window->DrawList->AddRectFilled(rowRect.Min, rowRect.Max, bgColor);
    }

    void HierarchyPanel::CollectVisibleEntities(SF::Engine::Entity *entity, std::vector<SF::Engine::Entity *> &outEntities)
    {
        if (entity)
        {
            outEntities.push_back(entity);
            for (auto &child : entity->GetChildren())
            {
                CollectVisibleEntities(child.get(), outEntities);
            }
        }
    }

    void HierarchyPanel::SetOnEntitySelected(std::function<void(SF::Engine::Entity *)> callback)
    {
        m_onEntitySelected = callback;
    }

    void HierarchyPanel::SetSelectedEntity(SF::Engine::Entity *entity)
    {
        if (entity)
        {
            m_selectedEntity = entity;
            m_selectedId = entity->GetId();
        }
        else
        {
            m_selectedEntity = nullptr;
            m_selectedId = 0;
        }
    }

    void HierarchyPanel::DrawCreateOptions()
    {
        EntityRegistry &registry = SceneManager::Get()->GetScene()->GetEntities()->GetRegistry();
        Scene *scene = SceneManager::Get()->GetScene();
        if (ImGui::BeginMenu("Create"))
        {
            if (ImGui::BeginMenu("Object"))
            {
                if (ImGui::MenuItem("Empty Entity"))
                {
                    registry.CreateEntity("New Entity");
                    m_needsRefresh = true;
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Cube"))
                {
                    SceneObject *cube = scene->AddObject("Cube");
                    cube->meshSourcePath = "__cube__";
                    m_needsRefresh = true;
                    ImGui::EndMenu();
                }
                // add more, also add more __mesh__ stuff
            }
            if (ImGui::BeginMenu("Light"))
            {
                if (ImGui::MenuItem("Directional Light"))
                {
                    scene->AddLight("Directional Light", Lighting::LightType::Directional, {1, 1, 1}, 10, {0, 0, 0}, {0, 0, 0});
                    m_needsRefresh = true;
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Point Light"))
                {
                    scene->AddLight("Point Light", Lighting::LightType::Point, {1, 1, 1}, 10, {0, 0, 0}, {0, 0, 0});
                    m_needsRefresh = true;
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Spot Light"))
                {
                    scene->AddLight("Spot Light", Lighting::LightType::Spot, {1, 1, 1}, 10, {0, 0, 0}, {0, 0, 0});
                    m_needsRefresh = true;
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
    }
}