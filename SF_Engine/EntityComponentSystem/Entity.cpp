#include "Entity.hpp"
#include <Math/Transform.hpp>

namespace SF::Engine
{
    Entity::Entity(const std::string &entityName) : name(entityName) { AddComponent<Transform>(); }

    // NOTE: moving a *registered* entity is not supported: the registry's lookup and
    // name index point at `other`'s address. The moved-to entity starts out unregistered
    // (ownerRegistry stays null). Registered entities should live in unique_ptrs and be
    // reparented via EntityRegistry::Reparent instead.
    Entity::Entity(Entity &&other) noexcept :
        components(std::move(other.components)), children(std::move(other.children)), tags(std::move(other.tags)),
        name(std::move(other.name)), id(other.id), parent(other.parent), markedForRemoval(other.markedForRemoval),
        active(other.active)
    {
        // Update parent pointers in moved children
        for (auto &child: children)
        {
            child->parent = this;
        }

        // Update owner pointers in moved components
        for (auto &[type, component]: components)
        {
            component->SetOwner(this);
        }

        // Reset the moved-from object
        other.parent           = nullptr;
        other.id               = 0;
        other.active           = false;
        other.markedForRemoval = false;
        other.ownerRegistry    = nullptr;
    }

    Entity &Entity::operator=(Entity &&other) noexcept
    {
        if (this != &other)
        {
            name             = std::move(other.name);
            tags             = std::move(other.tags);
            active           = other.active;
            components       = std::move(other.components);
            parent           = other.parent;
            children         = std::move(other.children);
            id               = other.id;
            markedForRemoval = other.markedForRemoval;
            ownerRegistry    = nullptr; // see note on the move constructor

            // Update parent pointers in moved children
            for (auto &child: children)
            {
                child->parent = this;
            }

            // Update owner pointers in moved components
            for (auto &[type, component]: components)
            {
                component->SetOwner(this);
            }

            // Reset the moved-from object
            other.parent           = nullptr;
            other.id               = 0;
            other.active           = false;
            other.markedForRemoval = false;
            other.ownerRegistry    = nullptr;
        }
        return *this;
    }

    void Entity::SetName(const std::string &newName)
    {
        if (ownerRegistry)
            ownerRegistry->RenameEntity(this, newName); // updates `name` and the index together
        else
            name = newName;
    }

    bool Entity::SetParent(Entity *newParent)
    {
        if (newParent == parent)
            return true;

        // Cycle iff newParent is this entity or lives inside its subtree.
        if (newParent && IsSelfOrDescendant(newParent))
        {
            Log::Warning("SetParent refused: '{}' can't be moved under itself or one of its descendants.", name);
            return false;
        }

        // Registered: the registry knows how to handle roots, null parents and its index.
        if (ownerRegistry)
            return ownerRegistry->Reparent(this, newParent);

        // Unregistered: we can only move between parents, since a bare Entity can't own itself.
        if (!parent || !newParent)
        {
            Log::Warning("SetParent refused on '{}': unregistered entities need both a current and a new parent. "
                         "Use EntityRegistry::Reparent for roots.",
                         name);
            return false;
        }

        std::unique_ptr<Entity> self = parent->ReleaseChild(this);
        if (!self)
            return false;

        newParent->AdoptChild(std::move(self));
        return true;
    }

    bool Entity::RemoveComponentByType(std::type_index ti)
    {
        if (ti == std::type_index(typeid(Transform)))
        {
            Log::Warning("Refusing to remove Transform, every entity must have one.");
            return false;
        }

        auto it = components.find(ti);
        if (it == components.end())
            return false;

        components.erase(it);
        return true;
    }
} // namespace SF::Engine
