#pragma once
#include <Entity/Components/Component.hpp>
#include <Entity/Entity.hpp>
#include <Gui/StaticPanel.hpp>
#include <Gui/UIRegistry.hpp>
#include <Gui/ocornut/imgui.h>
#include <functional>
#include <string>
namespace SF::Engine
{
    class EntityRegistry;

    class InspectorPanel : public StaticSingleInstancePanel<InspectorPanel>
    {
    public:
        InspectorPanel()
        {
            reg = UIRegistry::Get().Register([this] { Draw(); });
        };
        ~InspectorPanel() { UIRegistry::Get().Unregister(reg); };

        void Draw();
        void SetEntity(Entity *entity);
        [[nodiscard]] Entity *GetEntity() const { return m_entity; }
        void Refresh();

    private:
        void DrawEntityProperties();
        void DrawComponents();
        void DrawAddComponentMenu();
        bool DrawComponentField(const std::string &label, Component *component);

        Entity *m_entity           = nullptr;
        EntityId m_entityId        = 0;
        EntityRegistry *m_registry = nullptr;
        bool m_needsRefresh        = true;
        size_t reg;
    };
} // namespace SF::Engine
