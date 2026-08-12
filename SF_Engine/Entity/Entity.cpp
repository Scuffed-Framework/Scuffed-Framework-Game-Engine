#include "Entity.hpp"
#include <Math/Transform.hpp>

namespace SF::Engine
{
    Entity::Entity(const std::string &entityName, Entity *parent)
        : name(entityName), parent(parent)
    {
        AddComponent<Transform>();
    }

    Entity::Entity(Entity &&other) noexcept
        : name(std::move(other.name)), active(other.active), components(std::move(other.components)), parent(other.parent), children(std::move(other.children)), id(other.id), markedForRemoval(other.markedForRemoval)
    {
        // Update parent pointers in moved children
        for (auto &child : children)
        {
            child->parent = this;
        }

        // Update owner pointers in moved components
        for (auto &[type, component] : components)
        {
            component->SetOwner(this);
        }

        // Reset the moved-from object
        other.parent = nullptr;
        other.id = 0;
        other.active = false;
        other.markedForRemoval = false;
    }

    Entity &Entity::operator=(Entity &&other) noexcept
    {
        if (this != &other)
        {
            name = std::move(other.name);
            active = other.active;
            components = std::move(other.components);
            parent = other.parent;
            children = std::move(other.children);
            id = other.id;
            markedForRemoval = other.markedForRemoval;

            // Update parent pointers in moved children
            for (auto &child : children)
            {
                child->parent = this;
            }

            // Update owner pointers in moved components
            for (auto &[type, component] : components)
            {
                component->SetOwner(this);
            }

            // Reset the moved-from object
            other.parent = nullptr;
            other.id = 0;
            other.active = false;
            other.markedForRemoval = false;
        }
        return *this;
    }

    bool Entity::RemoveComponentByType(std::type_index ti)
    {
        if (ti == std::type_index(typeid(Transform)))
        {
            Log::Warning("Refusing to remove Transform — every entity must have one.");
            return false;
        }

        auto it = components.find(ti);
        if (it == components.end())
            return false;

        components.erase(it);
        return true;
    }
}