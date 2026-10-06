#pragma once

#include <EntityComponentSystem/Entity.hpp>
#include <Gui/ImGui/StaticPanel.hpp>
#include <Gui/ImGui/UIRegistry.hpp>
#include <Gui/ImGui/ocornut/imgui.h>
#include <functional>
#include <string>
#include <vector>

namespace SF::Engine
{
    class EntityRegistry;

    class HierarchyPanel : public StaticSingleInstancePanel<HierarchyPanel>
    {
    public:
        HierarchyPanel()
        {
            reg = UIRegistry::Get().Register([this] { Draw(); });
        };
        ~HierarchyPanel() { UIRegistry::Get().Unregister(reg); };

        void Draw();

        void SetOnEntitySelected(std::function<void(Entity *)> callback);
        void SetSelectedEntity(Entity *entity);
        [[nodiscard]] Entity *GetSelectedEntity() const { return m_selectedEntity; }
        [[nodiscard]] EntityId GetSelectedId() const { return m_selectedId; }

        void DrawCreateOptions();

    private:
        void DrawEntityNode(Entity *entity);
        void DrawRowBackground(float height);
        void CollectVisibleEntities(Entity *entity, std::vector<Entity *> &outEntities);

        Entity *m_selectedEntity = nullptr;
        EntityId m_selectedId    = 0;
        std::function<void(Entity *)> m_onEntitySelected;
        bool m_needsRefresh        = true;
        Entity *m_pendingDuplicate = nullptr;
        size_t reg;
    };
} // namespace SF::Engine
