#pragma once

#include <UtilityClasses/NoCopy.hpp>
#include <Entity/Components/Component.hpp>
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

        Entity *GetEntity(const std::string &name) const
        {
            return registry.FindByName(name);
        }

        Entity *FindById(const EntityId id)
        {
            return registry.Find(id);
        }

        template <typename T = Entity, typename... Args>
        T *CreateEntity(Args &&...args)
        {
            return registry.CreateEntity<T>(std::forward<Args>(args)...);
        }

        template <typename T = Entity, typename... Args>
        T *CreateChildEntity(Entity *parent, Args &&...args)
        {
            return registry.CreateChildEntity<T>(parent, std::forward<Args>(args)...);
        }

        Entity *CreateEntity(const std::string &name = "Entity")
        {
            return registry.CreateEntity(name);
        }

        Entity *CreateEntity(const std::string &name, Entity *parent)
        {
            return registry.CreateEntity(name, parent);
        }

        void Remove(Entity *entity)
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
            registry.ForEach([&](Entity *)
                             { ++count; });
            return count;
        }

        std::vector<Entity *> QueryAll() const
        {
            std::vector<Entity *> entities;
            registry.ForEach([&](Entity *e)
                             { entities.push_back(e); });
            return entities;
        }

        template <typename T>
        T *GetComponent(bool allowDisabled = false)
        {
            T *found = nullptr;
            registry.ForEach([&](Entity *e)
                             {
                if (found) return;
                if (auto* comp = e->GetComponent<T>())
                {
                    if (allowDisabled || comp->IsEnabled())
                        found = comp;
                } });
            return found;
        }

        template <typename T>
        std::vector<T *> QueryComponents(bool allowDisabled = false)
        {
            std::vector<T *> components;
            registry.ForEach([&](Entity *e)
                             {
                if (auto* comp = e->GetComponent<T>())
                {
                    if (allowDisabled || comp->IsEnabled())
                        components.push_back(comp);
                } });
            return components;
        }

        EntityRegistry &GetRegistry() { return registry; }
        const EntityRegistry &GetRegistry() const { return registry; }

        void Reparent(Entity *child, Entity *newParent)
        {
            if (!child || !newParent || child == newParent)
                return;

            std::unique_ptr<Entity> owned;

            if (Entity *oldParent = child->GetParent())
            {
                owned = oldParent->ReleaseChild(child);
            }
            else
            {
                owned = registry.RemoveRoot(child);
            }

            assert(owned && "Entity is not owned by the registry");

            newParent->AdoptChild(std::move(owned));
        }

    private:
        EntityRegistry registry;
    };
}