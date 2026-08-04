#pragma once

#include <UtilityClasses/NoCopy.hpp>
#include <Components/Component.hpp>
#include "Entity.hpp"
#include <vector>
#include <string>
#include <cstdint>

namespace SF::Engine
{
    class EntityHolder : NoCopy
    {
    public:
        EntityHolder() = default;

        void Update()
        {
            registry.CleanupRemovedEntities();
        }

        void CleanupRemovedEntities()
        {
            registry.CleanupRemovedEntities();
        }

        Entity* GetEntity(const std::string& name) const
        {
            return registry.FindByName(name);
        }

        Entity* CreateEntity(const std::string& name = "Entity")
        {
            return registry.CreateEntity(name);
        }

        Entity* CreateEntity(const std::string& name, Entity* parent)
        {
            return registry.CreateEntity(name, parent);
        }

        Entity* CreatePrefabEntity(const std::string& filename)
        {
            Entity* entity = registry.CreateEntity();
            // TODO: Load prefab data from file (XML deserialize into `entity`)
            return entity;
        }

        void Remove(Entity* entity)
        {
            registry.MarkForRemoval(entity);
        }

        void Clear()
        {
            registry = EntityRegistry{};
        }

        uint32_t GetSize() const
        {
            uint32_t count = 0;
            registry.ForEach([&](Entity*) { ++count; });
            return count;
        }

        std::vector<Entity*> QueryAll() const
        {
            std::vector<Entity*> entities;
            registry.ForEach([&](Entity* e) { entities.push_back(e); });
            return entities;
        }

        template <typename T>
        T* GetComponent(bool allowDisabled = false)
        {
            T* found = nullptr;
            registry.ForEach([&](Entity* e)
            {
                if (found) return;
                if (auto* comp = e->GetComponent<T>())
                {
                    if (allowDisabled || comp->IsEnabled())
                        found = comp;
                }
            });
            return found;
        }

        template <typename T>
        std::vector<T*> QueryComponents(bool allowDisabled = false)
        {
            std::vector<T*> components;
            registry.ForEach([&](Entity* e)
            {
                if (auto* comp = e->GetComponent<T>())
                {
                    if (allowDisabled || comp->IsEnabled())
                        components.push_back(comp);
                }
            });
            return components;
        }

        EntityRegistry& GetRegistry() { return registry; }
        const EntityRegistry& GetRegistry() const { return registry; }

    private:
        EntityRegistry registry;
    };
}