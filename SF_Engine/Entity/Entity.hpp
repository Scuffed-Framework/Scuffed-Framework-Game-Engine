#pragma once

#include <UtilityClasses/NoCopy.hpp>
#include <Entity/Components/Component.hpp>
#include <string>
#include <unordered_map>

namespace SF::Engine
{
    using EntityId = std::uint64_t;
    constexpr std::uint64_t InvalidEntityId = 0u; // should do the trick
    /**
     * @brief Tag component that presence/state of enabled flag. Entities
     * without it are implicitly enabled; only add when something toggles it.
     */
    struct EnabledComponent
    {
        bool enabled = true;
    };

    class Transform;
    // todo: cloning
    class Entity
    {
    public:
        Entity(const std::string &entityName, Entity *parent = nullptr);

        virtual ~Entity() = default;
        Entity(const Entity &) = delete;
        Entity &operator=(const Entity &) = delete;
        Entity(Entity &&other) noexcept;
        Entity &operator=(Entity &&other) noexcept;

        std::unordered_map<std::type_index, std::unique_ptr<Component>> components;

        std::vector<std::unique_ptr<Entity>> children;
        std::vector<std::string> tags = {};
        std::string name;

        EntityId id = 0;
        Entity *parent = nullptr;

        bool markedForRemoval = false;
        bool active = true;

        bool IsMarkedForRemoval() const { return markedForRemoval; }
        void MarkForRemoval() { markedForRemoval = true; }

        bool HasTag(std::string tag)
        {
            return std::find(tags.begin(), tags.end(), tag) != tags.end();
        }

        void AddTag(std::string tag)
        {
            if (std::find(tags.begin(), tags.end(), tag) == tags.end()) // not found
                tags.emplace_back(tag);
            else
                Log::Warning("Call to add tag to entity failed because the entity already has that tag."); // wont crash the engine so warn.
        }

        void RemoveTag(std::string tag)
        {
            if (std::find(tags.begin(), tags.end(), tag) != tags.end()) // found
                std::erase(tags, tag);
            else
                Log::Warning("Entity does not have the tag:{}", tag);
        }

        EntityId GetId() const { return id; }
        void SetId(EntityId newId) { id = newId; }
        void SetName(const std::string &newName) { name = newName; }

        // Used only by EntityRegistry::Reparent when detaching to root
        // bypasses the "must already have a parent" assert in SetParent().
        void SetParentRaw(Entity *p) { parent = p; }

        const std::string &GetName() const { return name; }
        bool IsActive() const { return active; }
        void SetActive(bool isActive) { active = isActive; }

        template <typename T, typename... Args>
        T *AddComponent(Args &&...args)
        {
            static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

            auto component = std::make_unique<T>(std::forward<Args>(args)...);
            T *componentPtr = component.get();
            componentPtr->SetOwner(this);
            components[std::type_index(typeid(T))] = std::move(component);
            return componentPtr;
        }

        template <typename T>
        T *GetComponent()
        {
            // Fast path: exact concrete type was stored under typeid(T).
            auto it = components.find(std::type_index(typeid(T)));
            if (it != components.end())
            {
                if (T *result = dynamic_cast<T *>(it->second.get()))
                    return result;
            }

            // Fallback: T might be a base/interface type (e.g. IRenderable)
            // that some other concrete component derives from.
            for (auto &[type, component] : components)
            {
                if (T *result = dynamic_cast<T *>(component.get()))
                    return result;
            }
            return nullptr;
        }

        template <typename T>
        const T *GetComponent() const
        {
            // Fast path: exact concrete type was stored under typeid(T).
            auto it = components.find(std::type_index(typeid(T)));
            if (it != components.end())
            {
                if (T *result = dynamic_cast<T *>(it->second.get()))
                    return result;
            }

            // Fallback: T might be a base/interface type (e.g. IRenderable)
            // that some other concrete component derives from.
            for (auto &[type, component] : components)
            {
                if (T *result = dynamic_cast<T *>(component.get()))
                    return result;
            }
            return nullptr;
        }

        template <typename T>
        bool RemoveComponent()
        {
            if constexpr (std::is_same_v<T, Transform>)
            {
                Log::Warning("Refusing to remove Transform, every entity must have one.");
                return false;
            }

            auto it = components.find(std::type_index(typeid(T)));
            if (it != components.end())
            {
                components.erase(it);
                return true;
            }
            // Fallback for base-type removal.
            for (auto it2 = components.begin(); it2 != components.end(); ++it2)
            {
                if (dynamic_cast<T *>(it2->second.get()))
                {
                    components.erase(it2);
                    return true;
                }
            }
            return false;
        }
        /**
         * @brief Type-erased component removal, keyed by the same std::type_index
         *        used to store it. Needed anywhere you only have a runtime type
         *        (e.g. iterating entity->components), since RemoveComponent<T>()
         *        requires a compile-time T.
         */
        bool RemoveComponentByType(std::type_index ti);

        template <typename T>
        bool HasComponent()
        {
            return GetComponent<T>() != nullptr;
        }

        bool HasComponent(std::string_view typeName) const
        {
            for (auto &[type, component] : components)
            {
                if (component->GetTypeName() == typeName)
                    return true;
            }
            return false;
        }

        Entity *GetParent() const { return parent; }

        const std::vector<std::unique_ptr<Entity>> &GetChildren() const { return children; }

        /**
         * @brief Constructs a brand-new entity of type T, owned by this one.
         *        T must derive from Entity. Parent is set after construction,
         *        so pass only T's non-parent constructor args here.
         */
        template <typename T = Entity, typename... Args>
        T *AddChild(Args &&...args)
        {
            static_assert(std::is_base_of<Entity, T>::value, "T must derive from Entity");
            auto child = std::make_unique<T>(std::forward<Args>(args)...);
            child->parent = this;
            T *ptr = child.get();
            children.push_back(std::move(child));
            return ptr;
        }

        /**
         * @brief Adopts an already-existing, currently-unparented entity
         *        (e.g. handed off from a Scene's root list or from ReleaseChild()).
         */
        Entity *AddChild(std::unique_ptr<Entity> child)
        {
            if (!child)
                return nullptr;
            assert(child->parent == nullptr && "Entity already has a parent; use SetParent() to reparent");

            child->parent = this;
            Entity *ptr = child.get();
            children.push_back(std::move(child));
            return ptr;
        }

        /**
         * @brief Adopts an already-existing, currently-unparented entity
         *        (e.g. handed off from a Scene's root list or from ReleaseChild()).
         *        Renamed from AddChild to avoid overload ambiguity with the
         *        templated AddChild<T>(Args&&...) above.
         */
        Entity *AdoptChild(std::unique_ptr<Entity> child)
        {
            if (!child)
                return nullptr;
            assert(child->parent == nullptr && "Entity already has a parent; use SetParent() to reparent");

            child->parent = this;
            Entity *ptr = child.get();
            children.push_back(std::move(child));
            return ptr;
        }

        /**
         * @brief Detaches `child` from this entity WITHOUT destroying it.
         *        Caller takes ownership. Returns nullptr if not found here.
         */
        std::unique_ptr<Entity> ReleaseChild(Entity *child)
        {
            auto it = std::find_if(children.begin(), children.end(),
                                   [child](const std::unique_ptr<Entity> &c)
                                   { return c.get() == child; });

            if (it == children.end())
                return nullptr;

            std::unique_ptr<Entity> released = std::move(*it);
            children.erase(it);
            released->parent = nullptr;
            return released;
        }

        /**
         * @brief Detaches and destroys `child` (and its whole subtree).
         */
        bool DestroyChild(Entity *child)
        {
            auto it = std::find_if(children.begin(), children.end(),
                                   [child](const std::unique_ptr<Entity> &c)
                                   { return c.get() == child; });

            if (it == children.end())
                return false;

            children.erase(it); // unique_ptr dtor cascades through descendants
            return true;
        }

        /**
         * @brief Returns true if `candidate` is this entity or one of its descendants.
         *        Used to prevent reparenting cycles.
         */
        bool IsSelfOrDescendant(const Entity *candidate) const
        {
            if (candidate == this)
                return true;
            for (auto &child : children)
            {
                if (child->IsSelfOrDescendant(candidate))
                    return true;
            }
            return false;
        }

        /**
         * @brief Moves this entity to be a child of `newParent`.
         *        Only works for entities that already have a parent
         *        (root entities owned by a Scene must be reparented via
         *        the Scene, since Entity doesn't own itself).
         */
        void SetParent(Entity *newParent)
        {
            if (newParent == parent)
                return;
            assert(newParent != this && "Entity cannot be its own parent");
            assert((newParent == nullptr || !newParent->IsSelfOrDescendant(this)) && "Reparenting would create a cycle");
            assert(parent != nullptr &&
                   "This entity has no current parent; reparent it via the owning Scene instead");

            std::unique_ptr<Entity> self = parent->ReleaseChild(this);
            if (newParent)
            {
                newParent->AddChild(std::move(self));
            }
            else
            {
                // Detaching to become a root entity: hand `self` off to
                // wherever your Scene keeps root entities.
                // e.g. scene->AdoptRoot(std::move(self));
            }
        }
    };

    struct NameComponent
    {
        std::string name;
    };

    class EntityRegistry
    {
    public:
        EntityRegistry() = default;

        /**
         * @brief Creates a new root entity of type T, owned directly by the registry.
         */
        template <typename T = Entity, typename... Args>
        T *CreateEntity(Args &&...args)
        {
            EntityId id = nextId++;
            auto entity = std::make_unique<T>(std::forward<Args>(args)...);
            entity->SetId(id);

            T *ptr = entity.get();
            lookup[id] = ptr;
            roots.push_back(std::move(entity));
            return ptr;
        }

        /**
         * @brief Creates a new entity of type T, parented under `parent`.
         *        Separate name from CreateEntity<T>(Args...) so root-vs-child
         *        creation can't be confused by overload resolution: a root
         *        entity is owned by `roots`, a child is owned by its parent's
         *        `children`; mixing those up leaves the registry inconsistent.
         */
        template <typename T = Entity, typename... Args>
        T *CreateChildEntity(Entity *parent, Args &&...args)
        {
            if (!parent)
                return CreateEntity<T>(std::forward<Args>(args)...);

            EntityId id = nextId++;
            T *ptr = parent->AddChild<T>(std::forward<Args>(args)...);
            ptr->SetId(id);
            lookup[id] = ptr;
            return ptr;
        }

        /**
         * @brief Creates a new entity parented under `parent`.
         *        Convenience wrapper so callers don't need to touch
         *        Entity::AddChild directly.
         */
        Entity *CreateEntity(const std::string &name, Entity *parent)
        {
            if (!parent)
                return CreateEntity(name);

            EntityId id = nextId++;
            Entity *ptr = parent->AddChild(name);
            ptr->SetId(id);
            lookup[id] = ptr;
            return ptr;
        }

        /**
         * @brief Destroys an entity (and its subtree), wherever it lives
         *        in the hierarchy.
         */
        void DestroyEntity(Entity *entity)
        {
            if (!entity)
                return;

            UnregisterSubtree(entity);

            if (Entity *parent = entity->GetParent())
            {
                parent->DestroyChild(entity);
            }
            else
            {
                auto it = std::find_if(roots.begin(), roots.end(),
                                       [entity](const std::unique_ptr<Entity> &e)
                                       { return e.get() == entity; });
                if (it != roots.end())
                    roots.erase(it);
            }
        }

        Entity *Find(EntityId id) const
        {
            auto it = lookup.find(id);
            return it != lookup.end() ? it->second : nullptr;
        }

        const std::vector<std::unique_ptr<Entity>> &GetRoots() const { return roots; }

        /**
         * @brief Reparents `entity` under `newParent` (or to root if
         *        newParent is nullptr), keeping the registry's lookup
         *        table consistent. This is the version of reparenting
         *        that Entity::SetParent alone can't do for root entities,
         *        since Entity doesn't own itself.
         */
        void Reparent(Entity *entity, Entity *newParent)
        {
            if (!entity || entity == newParent)
                return;
            assert((!newParent || !newParent->IsSelfOrDescendant(entity)) &&
                   "Reparenting would create a cycle");

            std::unique_ptr<Entity> owned;

            if (Entity *oldParent = entity->GetParent())
            {
                owned = oldParent->ReleaseChild(entity);
            }
            else
            {
                auto it = std::find_if(roots.begin(), roots.end(),
                                       [entity](const std::unique_ptr<Entity> &e)
                                       { return e.get() == entity; });
                assert(it != roots.end() && "Entity not tracked by this registry");
                owned = std::move(*it);
                roots.erase(it);
            }

            if (newParent)
            {
                newParent->AddChild(std::move(owned));
            }
            else
            {
                owned->SetParentRaw(nullptr); // just clears the pointer, no reparent logic needed
                roots.push_back(std::move(owned));
            }
        }

        /**
         * @brief Visits every entity in the registry, depth-first.
         */
        void ForEach(const std::function<void(Entity *)> &fn) const
        {
            for (auto &root : roots)
                VisitRecursive(root.get(), fn);
        }

        void MarkForRemoval(Entity *entity)
        {
            if (entity)
                entity->MarkForRemoval();
        }

        void CleanupRemovedEntities()
        {
            // Collect first; DestroyEntity mutates the containers we'd be iterating.
            std::vector<Entity *> toRemove;
            ForEach([&](Entity *e)
                    {
                if (e->IsMarkedForRemoval()) toRemove.push_back(e); });

            for (Entity *e : toRemove)
            {
                if (!IsValid(e))
                    continue;
                DestroyEntity(e);
            }
        }

        bool IsValid(Entity *entity) const
        {
            return entity != nullptr && lookup.count(entity->GetId()) != 0;
        }

    private:
        void VisitRecursive(Entity *entity, const std::function<void(Entity *)> &fn) const
        {
            fn(entity);
            for (auto &child : entity->GetChildren())
                VisitRecursive(child.get(), fn);
        }

        void UnregisterSubtree(Entity *entity)
        {
            lookup.erase(entity->GetId());
            for (auto &child : entity->GetChildren())
                UnregisterSubtree(child.get());
        }

        std::vector<std::unique_ptr<Entity>> roots;
        std::unordered_map<EntityId, Entity *> lookup;
        EntityId nextId = 2; // creates one at 1, and then 2, 0 = invalid.

        std::unordered_multimap<std::string, Entity *> nameIndex; // supports duplicate names

    public:
        Entity *FindByName(const std::string &name) const
        {
            auto it = nameIndex.find(name);
            return it != nameIndex.end() ? it->second : nullptr;
        }

        // Call this instead of entity->SetName() directly if you want
        // the registry's index to stay valid.
        void RenameEntity(Entity *entity, const std::string &newName)
        {
            RemoveFromNameIndex(entity);
            entity->SetName(newName);
            nameIndex.emplace(newName, entity);
        }

        std::unique_ptr<Entity> RemoveRoot(Entity *entity)
        {
            auto it = std::find_if(
                roots.begin(),
                roots.end(),
                [entity](const std::unique_ptr<Entity> &root)
                {
                    return root.get() == entity;
                });

            if (it == roots.end())
                return nullptr;

            auto owned = std::move(*it);
            roots.erase(it);

            owned->SetParentRaw(nullptr);

            return owned;
        }

    private:
        void RemoveFromNameIndex(Entity *entity)
        {
            auto range = nameIndex.equal_range(entity->GetName());
            for (auto it = range.first; it != range.second; ++it)
            {
                if (it->second == entity)
                {
                    nameIndex.erase(it);
                    return;
                }
            }
        }
    };
}