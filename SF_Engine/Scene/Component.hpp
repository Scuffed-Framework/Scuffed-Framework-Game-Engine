#pragma once

#include <UtilityClasses/StreamFactory.hpp>
#include "Entity.hpp"

namespace SF::Engine
{
    /**
     * @brief Class that represents a functional component attached to entity.
     */
    class Component : public StreamFactory<Component>  // No Args -_-
    {
    public:
        virtual ~Component() = default;

        virtual void Start() {}
        virtual void Update() {}
        virtual TypeId GetTypeId() const = 0;
        virtual std::string_view GetTypeName() const = 0;

        bool IsEnabled() const
        {
            return enabled;
        }
        void SetEnabled(bool enable)
        {
            this->enabled = enable;
        }

        bool IsRemoved() const
        {
            return removed;
        }
        void SetRemoved(bool removed)
        {
            this->removed = removed;
        }

        Entity* GetEntity() const
        {
            return entity;
        }
        void SetEntity(Entity* entity)
        {
            this->entity = entity;
        }

    private:
        bool started = false;
        bool enabled = true;
        bool removed = false;
        Entity* entity = nullptr;
    };
}