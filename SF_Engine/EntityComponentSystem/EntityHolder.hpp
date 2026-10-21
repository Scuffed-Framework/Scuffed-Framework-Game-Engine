#pragma once

#include <EntityComponentSystem/Component.hpp>
#include <UtilityClasses/NoCopy.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>
#include "Entity.hpp"

namespace SF::Engine
{
    class EntityHolder : NoCopy
    {
    public:
        EntityHolder() = default;

        void Update() { registry.CleanupRemovedEntities(); }

        void CleanupRemovedEntities() { registry.CleanupRemovedEntities(); }

        Entity *GetEntity(const std::string &name) const { return registry.FindByName(name); }

        std::vector<Entity *> GetEntities(const std::string &name) const { return registry.FindAllByName(name); }

        Entity *FindById(const EntityId id) { return registry.Find(id); }

        Entity *Duplicate(Entity *entity) { return registry.DuplicateEntity(entity); }

        // Constrained so e.g. CreateEntity("name", parent) isn't captured by this template
        // (exact match on the string literal) and instead reaches the (name, parent) overload.
        template<typename T = Entity, typename... Args>
            requires std::is_constructible_v<T, Args...>
        T *CreateEntity(Args &&...args)
        {
            return registry.CreateEntity<T>(std::forward<Args>(args)...);
        }

        template<typename T = Entity, typename... Args>
            requires std::is_constructible_v<T, Args...>
        T *CreateChildEntity(Entity *parent, Args &&...args)
        {
            return registry.CreateChildEntity<T>(parent, std::forward<Args>(args)...);
        }

        Entity *CreateEntity(const std::string &name = "Entity") { return registry.CreateEntity(name); }

        Entity *CreateEntity(const std::string &name, Entity *parent) { return registry.CreateEntity(name, parent); }

        void Remove(Entity *entity) { registry.MarkForRemoval(entity); }

        void Clear() { registry.Clear(); }

        uint32_t GetSize() const
        {
            uint32_t count = 0;
            registry.ForEach([&](Entity *) { ++count; });
            return count;
        }

        std::vector<Entity *> QueryAll() const
        {
            std::vector<Entity *> entities;
            registry.ForEach([&](Entity *e) { entities.push_back(e); });
            return entities;
        }

        template<typename T>
        T *GetComponent(bool allowDisabled = false)
        {
            T *found = nullptr;
            registry.ForEach(
                    [&](Entity *e)
                    {
                        if (found)
                            return;
                        if (auto *comp = e->GetComponent<T>())
                        {
                            if (allowDisabled || comp->IsEnabled())
                                found = comp;
                        }
                    });
            return found;
        }

        template<typename T>
        std::vector<T *> QueryComponents(bool allowDisabled = false)
        {
            std::vector<T *> components;
            registry.ForEach(
                    [&](Entity *e)
                    {
                        if (auto *comp = e->GetComponent<T>())
                        {
                            if (allowDisabled || comp->IsEnabled())
                                components.push_back(comp);
                        }
                    });
            return components;
        }

        EntityRegistry &GetRegistry() { return registry; }
        const EntityRegistry &GetRegistry() const { return registry; }

        /**
         * @brief Moves `child` under `newParent`, or to the root list if newParent is nullptr.
         *        Delegates to the registry, which refuses cycles and keeps lookup/name index
         *        consistent. Returns false if the move was refused.
         */
        bool Reparent(Entity *child, Entity *newParent) { return registry.Reparent(child, newParent); }

    private:
        EntityRegistry registry;
    };
} // namespace SF::Engine
