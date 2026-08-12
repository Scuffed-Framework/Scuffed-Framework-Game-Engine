#pragma once

#include <Gui/ocornut/imgui.h>
#include <string>
#include <functional>
#include <vector>
#include <Entity/Entity.hpp>
#include <Gui/UIRegistry.hpp>
#include <TemplateLibrary/Types.hpp>
#include <Gui/StaticPanel.hpp>
namespace SF::Engine
{
    class EntityRegistry;

    class HierarchyPanel : public StaticSingleInstancePanel<HierarchyPanel>
    {
    public:
        HierarchyPanel(){ reg = UIRegistry::Get().Register([this]{Draw();});};
        ~HierarchyPanel() {UIRegistry::Get().Unregister(reg);};

        void Draw();
        
        void SetOnEntitySelected(std::function<void(SF::Engine::Entity*)> callback);
        void SetSelectedEntity(SF::Engine::Entity* entity);
        SF::Engine::Entity* GetSelectedEntity() const { return m_selectedEntity; }
        EntityId GetSelectedId() const { return m_selectedId; }

        void DrawCreateOptions();

    private:
        void DrawEntityNode(SF::Engine::Entity* entity);
        void DrawRowBackground(float height);
        void CollectVisibleEntities(SF::Engine::Entity* entity, std::vector<SF::Engine::Entity*>& outEntities);

        SF::Engine::Entity* m_selectedEntity = nullptr;
        EntityId m_selectedId = 0;
        std::function<void(SF::Engine::Entity*)> m_onEntitySelected;
        bool m_needsRefresh = true;
        ::SFTL::size_t reg;
    };
}