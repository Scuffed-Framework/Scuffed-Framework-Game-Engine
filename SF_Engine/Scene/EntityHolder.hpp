#pragma once

#include <UtilityClasses/NoCopy.hpp>
#include "Component.hpp"
#include "Entity.hpp"

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

        Entity GetEntity(const std::string& name) const
        {
            auto view = registry.View<NameComponent>();
            for (auto entity : view)
            {
                const auto& nameComp = view.get<NameComponent>(entity);
                if (nameComp.name == name) return entity;
            }
            return entt::null;
        }

        Entity CreateEntity()
        {
            return registry.CreateEntity();
        }

        Entity CreatePrefabEntity(const std::string& filename)
        {
            auto entity = registry.CreateEntity();
            // TODO: Load prefab data from file
            return entity;
        }

        void Remove(Entity entity)
        {
            registry.MarkForRemoval(entity);
        }

        void Clear()
        {
            registry.GetRegistry().clear();
        }

        uint32_t GetSize() const
        {
            uint32_t count = 0;

            auto view = registry.GetRegistry().view<entt::entity>();
            for (auto entity : view) count++;

            return count;
        }

        std::vector<Entity> QueryAll()
        {
            std::vector<Entity> entities;

            auto view = registry.GetRegistry().view<entt::entity>();
            for (auto entity : view) entities.push_back(entity);

            return entities;
        }

        template <typename T>
        T* GetComponent(bool allowDisabled = false)
        {
            auto view = registry.View<T>();
            for (auto entity : view)
            {
                return registry.GetComponent<T>(entity);
            }
            return nullptr;
        }

        template <typename T>
        std::vector<T*> QueryComponents(bool allowDisabled = false)
        {
            std::vector<T*> components;
            auto view = registry.View<T>();
            for (auto entity : view)
            {
                if (auto* comp = registry.GetComponent<T>(entity))
                {
                    components.push_back(comp);
                }
            }
            return components;
        }

        EntityRegistry& GetRegistry()
        {
            return registry;
        }
        const EntityRegistry& GetRegistry() const
        {
            return registry;
        }

    private:
        EntityRegistry registry;
    };
}