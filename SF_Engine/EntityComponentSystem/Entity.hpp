#pragma once

#include <EntityComponentSystem/Component.hpp>
#include <UtilityClasses/NoCopy.hpp>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace SF::Engine
{
    using namespace std;
    using EntityId                          = uint64_t;
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
    class EntityRegistry;

    class Entity
    {
    public:
        /**
         * @brief Parenting is NOT done through the constructor. Use
         *        Entity::AddChild / EntityRegistry::CreateChildEntity, which
         *        set the parent and put the entity into the parent's children.
         *        (A constructor-side parent pointer left the entity claiming a
         *        parent that didn't own it.)
         */
        explicit Entity(const string &entityName);

        virtual ~Entity()                 = default;
        Entity(const Entity &)            = delete;
        Entity &operator=(const Entity &) = delete;
        Entity(Entity &&other) noexcept;
        Entity &operator=(Entity &&other) noexcept;

        unordered_map<type_index, unique_ptr<Component>> components;

        vector<unique_ptr<Entity>> children;
        vector<string> tags = {};
        string name;

        EntityId id    = 0;
        Entity *parent = nullptr;

        bool markedForRemoval = false;
        bool active           = true;

        bool IsMarkedForRemoval() const { return markedForRemoval; }
        void MarkForRemoval() { markedForRemoval = true; }

        bool HasTag(const std::string &tag) const { return ranges::find(tags, tag) != tags.end(); }

        void AddTag(const std::string &tag)
        {
            if (ranges::find(tags, tag) == tags.end()) // not found
                tags.emplace_back(tag);
            else
                Log::Warning(
                        "Call to add tag to entity failed because the entity already has that tag."); // wont crash the
                                                                                                      // engine so warn.
        }

        void RemoveTag(const std::string &tag)
        {
            if (ranges::find(tags, tag) != tags.end()) // found
                std::erase(tags, tag);
            else
                Log::Warning("Entity does not have the tag:{}", tag);
        }

        EntityId GetId() const { return id; }
        void SetId(EntityId newId) { id = newId; }

        /**
         * @brief Sets the name. If the entity is registered, this routes through
         *        EntityRegistry::RenameEntity so the name index stays valid.
         *        Defined in Entity.cpp.
         */
        void SetName(const std::string &newName);

        // Used only by EntityRegistry when detaching to root.
        // bypasses the "must already have a parent" assert in SetParent().
        void SetParentRaw(Entity *p) { parent = p; }

        const std::string &GetName() const { return name; }
        bool IsActive() const { return active; }
        void SetActive(bool isActive) { active = isActive; }

        template<typename T, typename... Args>
        T *AddComponent(Args &&...args)
        {
            static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

            auto component  = std::make_unique<T>(std::forward<Args>(args)...);
            T *componentPtr = component.get();
            componentPtr->SetOwner(this);
            components[std::type_index(typeid(T))] = std::move(component);
            return componentPtr;
        }

        /**
         * @brief Factory hook so derived entity types clone as themselves.
         *        Override in subclasses (and copy any extra members there).
         */
        virtual std::unique_ptr<Entity> CreateInstance() const { return std::make_unique<Entity>(name); }

        /**
         * @brief Deep-copies this entity: name, tags, active flag, components,
         *        and the full child subtree. The result is unparented, has
         *        id == InvalidEntityId, and is NOT registered anywhere.
         *        Use EntityRegistry::DuplicateEntity() to place it in the scene.
         */
        std::unique_ptr<Entity> Clone() const
        {
            auto copy    = CreateInstance();
            copy->name   = name;
            copy->tags   = tags;
            copy->active = active;
            // markedForRemoval intentionally not copied

            for (const auto &[ti, comp]: components)
            {
                auto c = comp->Clone();
                if (!c)
                {
                    Log::Warning("Component '{}' is not cloneable, skipped.", comp->GetTypeName());
                    continue;
                }
                c->SetOwner(copy.get());
                copy->components[ti] = std::move(c); // replaces the default Transform, if the ctor adds one
            }

            for (const auto &child: children)
                copy->AdoptChild(child->Clone()); // sets child->parent = copy

            return copy;
        }

        template<typename T>
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
            for (auto &component: components | views::values)
            {
                if (T *result = dynamic_cast<T *>(component.get()))
                    return result;
            }
            return nullptr;
        }

        template<typename T>
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
            for (const auto &component: components | views::values)
            {
                if (T *result = dynamic_cast<T *>(component.get()))
                    return result;
            }
            return nullptr;
        }

        template<typename T>
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

            // Fallback for base-type removal. Goes through RemoveComponentByType so
            // a base type (e.g. RemoveComponent<Component>()) can never take out the
            // Transform by matching it via dynamic_cast.
            for (const auto &[ti, comp]: components)
            {
                if (dynamic_cast<T *>(comp.get()))
                    return RemoveComponentByType(ti);
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

        template<typename T>
        bool HasComponent()
        {
            return GetComponent<T>() != nullptr;
        }

        bool HasComponent(std::string_view typeName) const
        {
            for (const auto &component: components | views::values)
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
        template<typename T = Entity, typename... Args>
            requires std::is_constructible_v<T, Args...>
        T *AddChild(Args &&...args)
        {
            static_assert(std::is_base_of_v<Entity, T>, "T must derive from Entity");
            auto child    = std::make_unique<T>(std::forward<Args>(args)...);
            child->parent = this;
            T *ptr        = child.get();
            children.push_back(std::move(child));
            return ptr;
        }

        /**
         * @brief Adopts an already-existing, currently-unparented entity
         *        (e.g. handed off from a Scene's root list or from ReleaseChild()).
         */
        Entity *AddChild(std::unique_ptr<Entity> child) { return AdoptChild(std::move(child)); }

        /**
         * @brief Adopts an already-existing, currently-unparented entity
         *        (e.g. handed off from a Scene's root list or from ReleaseChild()).
         */
        Entity *AdoptChild(std::unique_ptr<Entity> child)
        {
            if (!child)
                return nullptr;
            assert(child->parent == nullptr && "Entity already has a parent; use SetParent() to reparent");

            child->parent = this;
            Entity *ptr   = child.get();
            children.push_back(std::move(child));
            return ptr;
        }

        /**
         * @brief Detaches `child` from this entity WITHOUT destroying it.
         *        Caller takes ownership. Returns nullptr if not found here.
         */
        std::unique_ptr<Entity> ReleaseChild(Entity *child)
        {
            auto it = ranges::find_if(children, [child](const std::unique_ptr<Entity> &c) { return c.get() == child; });

            if (it == children.end())
                return nullptr;

            std::unique_ptr<Entity> released = std::move(*it);
            children.erase(it);
            released->parent = nullptr;
            return released;
        }

        /**
         * @brief Detaches and destroys `child` (and its whole subtree).
         *        NOTE: this does not touch any registry. For registered entities
         *        use EntityRegistry::DestroyEntity so lookup/name index are cleaned.
         */
        bool DestroyChild(Entity *child)
        {
            auto it = ranges::find_if(children, [child](const std::unique_ptr<Entity> &c) { return c.get() == child; });

            if (it == children.end())
                return false;

            children.erase(it); // unique_ptr dtor cascades through descendants
            return true;
        }

        /**
         * @brief Returns true if `candidate` is this entity or one of its descendants.
         *        Reparenting `X` under `P` would create a cycle exactly when
         *        X->IsSelfOrDescendant(P) is true.
         */
        bool IsSelfOrDescendant(const Entity *candidate) const
        {
            if (candidate == this)
                return true;
            for (auto &child: children)
            {
                if (child->IsSelfOrDescendant(candidate))
                    return true;
            }
            return false;
        }

        /**
         * @brief Moves this entity under `newParent` (nullptr = make it a root).
         *        Registered entities are forwarded to EntityRegistry::Reparent
         *        (handles roots and the index). Unregistered entities must already
         *        have a parent and a non-null newParent.
         *        Returns false (and changes nothing) if the move was refused.
         *        Defined in Entity.cpp.
         */
        bool SetParent(Entity *newParent);

    private:
        friend class EntityRegistry;

        // Set by EntityRegistry while this entity is registered, null otherwise.
        // Lets SetName / SetParent keep the registry consistent.
        EntityRegistry *ownerRegistry = nullptr;
    };

    struct NameComponent
    {
        std::string name;
    };

    class EntityRegistry
    {
    public:
        EntityRegistry() = default;

        EntityRegistry(const EntityRegistry &)            = delete;
        EntityRegistry &operator=(const EntityRegistry &) = delete;

        // Entities hold a back-pointer to their registry, so moving must re-point them.
        EntityRegistry(EntityRegistry &&other) noexcept :
            roots(std::move(other.roots)), lookup(std::move(other.lookup)), nameIndex(std::move(other.nameIndex)),
            nextId(other.nextId)
        {
            other.Clear();
            Rebind();
        }

        EntityRegistry &operator=(EntityRegistry &&other) noexcept
        {
            if (this != &other)
            {
                roots     = std::move(other.roots);
                lookup    = std::move(other.lookup);
                nameIndex = std::move(other.nameIndex);
                nextId    = other.nextId;
                other.Clear();
                Rebind();
            }
            return *this;
        }

        /**
         * @brief Destroys every entity and resets the registry to its initial state.
         */
        void Clear()
        {
            roots.clear();
            lookup.clear();
            nameIndex.clear();
            nextId = 2;
        }

        /**
         * @brief Creates a new root entity of type T, owned directly by the registry.
         *        The constraint keeps calls like CreateEntity("name", parent) from being
         *        hijacked by this template (string literal = exact match) when T can't be
         *        built from those args; they fall through to the (name, parent) overload.
         */
        template<typename T = Entity, typename... Args>
            requires std::is_constructible_v<T, Args...>
        T *CreateEntity(Args &&...args)
        {
            auto entity = std::make_unique<T>(std::forward<Args>(args)...);
            T *ptr      = entity.get();
            RegisterEntity(ptr);
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
        template<typename T = Entity, typename... Args>
            requires std::is_constructible_v<T, Args...>
        T *CreateChildEntity(Entity *parent, Args &&...args)
        {
            if (!parent)
                return CreateEntity<T>(std::forward<Args>(args)...);

            T *ptr = parent->AddChild<T>(std::forward<Args>(args)...);
            RegisterEntity(ptr);
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

            Entity *ptr = parent->AddChild(name);
            RegisterEntity(ptr);
            return ptr;
        }

        /**
         * @brief Destroys an entity (and its subtree), wherever it lives
         *        in the hierarchy.
         */
        void DestroyEntity(Entity *entity)
        {
            if (!IsValid(entity))
                return;

            // Unregister first: this also clears ids/back-pointers while the
            // whole subtree is still alive.
            UnregisterSubtree(entity);

            if (Entity *parent = entity->GetParent())
            {
                parent->DestroyChild(entity);
            } else
            {
                auto it = ranges::find_if(roots,
                                          [entity](const std::unique_ptr<Entity> &e) { return e.get() == entity; });
                if (it != roots.end())
                    roots.erase(it);
            }
        }

        Entity *Find(EntityId id) const
        {
            auto it = lookup.find(id);
            return it != lookup.end() ? it->second : nullptr;
        }

        /**
         * @brief Returns one entity with this name, or nullptr. With duplicate
         *        names, which one you get is unspecified; use FindAllByName.
         */
        Entity *FindByName(const std::string &name) const
        {
            auto it = nameIndex.find(name);
            return it != nameIndex.end() ? it->second : nullptr;
        }

        std::vector<Entity *> FindAllByName(const std::string &name) const
        {
            std::vector<Entity *> out;
            auto [first, last] = nameIndex.equal_range(name);
            for (auto it = first; it != last; ++it)
                out.push_back(it->second);
            return out;
        }

        const std::vector<std::unique_ptr<Entity>> &GetRoots() const { return roots; }

        /**
         * @brief Reparents `entity` under `newParent` (or to root if
         *        newParent is nullptr), keeping the registry consistent.
         *        Returns false and changes nothing if either entity isn't
         *        registered here, or if the move would create a cycle.
         */
        bool Reparent(Entity *entity, Entity *newParent)
        {
            if (!IsValid(entity) || (newParent && !IsValid(newParent)))
                return false;

            if (entity->GetParent() == newParent)
                return true; // already there (also covers root -> root)

            // Cycle iff newParent is the entity itself or lives inside its subtree.
            if (newParent && entity->IsSelfOrDescendant(newParent))
            {
                Log::Warning("Reparent refused: '{}' can't be moved under itself or one of its descendants.",
                             entity->GetName());
                return false;
            }

            std::unique_ptr<Entity> owned;

            if (Entity *oldParent = entity->GetParent())
            {
                owned = oldParent->ReleaseChild(entity);
            } else
            {
                auto it = ranges::find_if(roots,
                                          [entity](const std::unique_ptr<Entity> &e) { return e.get() == entity; });
                if (it != roots.end())
                {
                    owned = std::move(*it);
                    roots.erase(it);
                }
            }

            if (!owned)
            {
                Log::Warning("Reparent failed: '{}' isn't owned where the registry expected.", entity->GetName());
                return false;
            }

            if (newParent)
                newParent->AdoptChild(std::move(owned));
            else
                roots.push_back(std::move(owned)); // ReleaseChild already nulled the parent pointer

            return true;
        }

        /**
         * @brief Visits every entity in the registry, depth-first.
         */
        void ForEach(const std::function<void(Entity *)> &fn) const
        {
            for (auto &root: roots)
                VisitRecursive(root.get(), fn);
        }

        void MarkForRemoval(Entity *entity)
        {
            if (entity)
                entity->MarkForRemoval();
        }

        void CleanupRemovedEntities()
        {
            // Collect IDs, not pointers: destroying a marked parent also destroys
            // marked descendants, and a raw pointer to one of those would dangle.
            std::vector<EntityId> toRemove;
            ForEach(
                    [&](Entity *e)
                    {
                        if (e->IsMarkedForRemoval())
                            toRemove.push_back(e->GetId());
                    });

            for (EntityId id: toRemove)
            {
                if (Entity *e = Find(id)) // null if already destroyed with an ancestor
                    DestroyEntity(e);
            }
        }

        /**
         * @brief True only if `entity` is the very entity registered under its id.
         */
        bool IsValid(const Entity *entity) const { return entity != nullptr && Find(entity->GetId()) == entity; }

        // Prefer entity->SetName(), which routes here when the entity is registered.
        void RenameEntity(Entity *entity, const std::string &newName)
        {
            if (!entity)
                return;

            if (!IsValid(entity)) // not ours: nothing to index
            {
                entity->name = newName;
                return;
            }

            if (entity->name == newName)
                return;

            RemoveFromNameIndex(entity);
            entity->name = newName;
            nameIndex.emplace(newName, entity);
        }

        /**
         * @brief Takes a root entity (and its subtree) OUT of the registry and hands
         *        ownership to the caller. The subtree is unregistered (ids cleared,
         *        removed from lookup and name index) so nothing here can dangle.
         *        To move an entity under another parent, use Reparent instead.
         *        Give it back with AdoptRoot.
         */
        std::unique_ptr<Entity> RemoveRoot(Entity *entity)
        {
            auto it = ranges::find_if(roots,
                                      [entity](const std::unique_ptr<Entity> &root) { return root.get() == entity; });

            if (it == roots.end())
                return nullptr;

            auto owned = std::move(*it);
            roots.erase(it);

            UnregisterSubtree(owned.get());
            owned->SetParentRaw(nullptr);

            return owned;
        }

        /**
         * @brief Registers an unparented entity (and its subtree) as a new root,
         *        assigning fresh ids and indexing names. Counterpart of RemoveRoot.
         */
        Entity *AdoptRoot(std::unique_ptr<Entity> entity)
        {
            if (!entity)
                return nullptr;
            assert(entity->GetParent() == nullptr && "AdoptRoot needs an unparented entity");

            Entity *ptr = entity.get();
            RegisterSubtree(ptr);
            roots.push_back(std::move(entity));
            return ptr;
        }

        Entity *DuplicateEntity(Entity *source)
        {
            if (!source)
                return nullptr;

            auto copy = source->Clone();
            copy->SetName(source->GetName() + " (Copy)"); // only the top-level copy is renamed

            Entity *ptr = copy.get();
            RegisterSubtree(ptr);

            if (Entity *parent = source->GetParent())
                parent->AdoptChild(std::move(copy));
            else
                roots.push_back(std::move(copy));

            return ptr;
        }

    private:
        void VisitRecursive(Entity *entity, const std::function<void(Entity *)> &fn) const
        {
            fn(entity);
            for (auto &child: entity->GetChildren())
                VisitRecursive(child.get(), fn);
        }

        // The single place an entity enters lookup + name index.
        void RegisterEntity(Entity *entity)
        {
            const EntityId id = nextId++;
            entity->SetId(id);
            entity->ownerRegistry = this;
            lookup[id]            = entity;
            nameIndex.emplace(entity->GetName(), entity);
        }

        void RegisterSubtree(Entity *entity)
        {
            RegisterEntity(entity);
            for (auto &child: entity->GetChildren())
                RegisterSubtree(child.get());
        }

        // The single place an entity leaves lookup + name index.
        void UnregisterSubtree(Entity *entity)
        {
            RemoveFromNameIndex(entity);
            lookup.erase(entity->GetId());
            entity->SetId(InvalidEntityId);
            entity->ownerRegistry = nullptr;
            for (auto &child: entity->GetChildren())
                UnregisterSubtree(child.get());
        }

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

            // Fallback: `name` is a public member, so someone may have renamed the
            // entity without going through SetName. Find it by pointer instead.
            for (auto it = nameIndex.begin(); it != nameIndex.end(); ++it)
            {
                if (it->second == entity)
                {
                    nameIndex.erase(it);
                    return;
                }
            }
        }

        // Re-point every entity's back-pointer at this registry (after a move).
        void Rebind()
        {
            ForEach([this](Entity *e) { e->ownerRegistry = this; });
        }

        std::vector<std::unique_ptr<Entity>> roots;
        std::unordered_map<EntityId, Entity *> lookup;
        std::unordered_multimap<std::string, Entity *> nameIndex; // supports duplicate names
        EntityId nextId = 2;                                      // creates one at 1, and then 2, 0 = invalid.
    };
} // namespace SF::Engine
