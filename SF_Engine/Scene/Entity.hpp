#pragma once

#include <Scene/entt.hpp>
#include <string>

namespace SF::Engine
{
    using Entity = ::entt::entity; // msvc is a little dumb
    // Components are now just plain data structures
    struct NameComponent
    {
        ::std::string name; // msvc having a schizo outbreak
    };

    struct RemovedComponent
    {
        // Tag component - presence indicates entity is marked for removal
    };

    /**
     * @brief Registry wrapper that manages entities and components using EnTT.
     */
    class EntityRegistry
    {
    public:
        EntityRegistry() = default;

        /**
         * Creates a new entity.
         * @return The entity handle.
         */
        Entity CreateEntity()
        {
            return registry.create();
        }

        /**
         * Creates a named entity.
         * @param name The entity name.
         * @return The entity handle.
         */
        Entity CreateEntity(const std::string &name)
        {
            auto entity = registry.create();
            registry.emplace<NameComponent>(entity, name);
            return entity;
        }

        /**
         * Destroys an entity and all its components.
         * @param entity The entity to destroy.
         */
        void DestroyEntity(Entity entity)
        {
            if (registry.valid(entity))
            {
                registry.destroy(entity);
            }
        }

        /**
         * Marks an entity for removal.
         * @param entity The entity to mark.
         */
        void MarkForRemoval(Entity entity)
        {
            if (registry.valid(entity))
            {
                registry.emplace_or_replace<RemovedComponent>(entity);
            }
        }

        /**
         * Checks if an entity is marked for removal.
         * @param entity The entity to check.
         * @return True if marked for removal.
         */
        bool IsMarkedForRemoval(Entity entity) const
        {
            return registry.valid(entity) && registry.all_of<RemovedComponent>(entity);
        }

        /**
         * Gets the name of an entity.
         * @param entity The entity.
         * @return The entity name, or empty string if no name.
         */
        const std::string &GetName(Entity entity) const
        {
            static const std::string empty;
            if (registry.valid(entity))
            {
                if (auto *name = registry.try_get<NameComponent>(entity))
                {
                    return name->name;
                }
            }
            return empty;
        }

        /**
         * Sets the name of an entity.
         * @param entity The entity.
         * @param name The new name.
         */
        void SetName(Entity entity, const std::string &name)
        {
            if (registry.valid(entity))
            {
                registry.emplace_or_replace<NameComponent>(entity, name);
            }
        }

        /**
         * Adds a component to an entity.
         * @tparam T The component type.
         * @tparam Args Constructor argument types.
         * @param entity The entity.
         * @param args Constructor arguments.
         * @return Reference to the added component.
         */
        template <typename T, typename... Args>
        T &AddComponent(Entity entity, Args &&...args)
        {
            return registry.emplace<T>(entity, std::forward<Args>(args)...);
        }

        /**
         * Gets a component from an entity.
         * @tparam T The component type.
         * @param entity The entity.
         * @return Pointer to component, or nullptr if not present.
         */
        template <typename T>
        T *GetComponent(Entity entity)
        {
            return registry.try_get<T>(entity);
        }

        /**
         * Gets a component from an entity (const version).
         * @tparam T The component type.
         * @param entity The entity.
         * @return Pointer to component, or nullptr if not present.
         */
        template <typename T>
        const T *GetComponent(Entity entity) const
        {
            return registry.try_get<T>(entity);
        }

        /**
         * Checks if an entity has a component.
         * @tparam T The component type.
         * @param entity The entity.
         * @return True if component exists.
         */
        template <typename T>
        bool HasComponent(Entity entity) const
        {
            return registry.valid(entity) && registry.all_of<T>(entity);
        }

        /**
         * Removes a component from an entity.
         * @tparam T The component type.
         * @param entity The entity.
         */
        template <typename T>
        void RemoveComponent(Entity entity)
        {
            if (registry.valid(entity))
            {
                registry.remove<T>(entity);
            }
        }

        /**
         * Gets a view of all entities with specified components.
         * @tparam Components The component types to filter by.
         * @return EnTT view for iteration.
         */
        template <typename... Components>
        auto View()
        {
            return registry.view<Components...>();
        }

        /**
         * Gets a const view of all entities with specified components.
         * @tparam Components The component types to filter by.
         * @return EnTT view for iteration.
         */
        template <typename... Components>
        auto View() const
        {
            return registry.view<Components...>();
        }

        /**
         * Destroys all entities marked for removal.
         */
        void CleanupRemovedEntities()
        {
            auto view = registry.view<RemovedComponent>();
            registry.destroy(view.begin(), view.end());
        }

        /**
         * Gets the underlying EnTT registry.
         * @return Reference to the registry.
         */
        entt::registry &GetRegistry()
        {
            return registry;
        }
        const entt::registry &GetRegistry() const
        {
            return registry;
        }

        void Clear()
        {
            registry.clear();
        }

    private:
        entt::registry registry;
    };

}