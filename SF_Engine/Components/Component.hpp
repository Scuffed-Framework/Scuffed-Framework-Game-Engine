#pragma once

#include <UtilityClasses/StreamFactory.hpp>
#include <XML/XMLModule.hpp>

namespace SF::Engine
{
    class Entity;
    /**
     * @brief Class that represents a functional component attached to entity.
     */
    class Component : public StreamFactory<Component>, public Serializable
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

        Entity* GetOwner() const
        {
            return owner;
        }
        void SetOwner(Entity* entity)
        {
            this->owner = entity;
        }

    private:
        bool started = false;
        bool enabled = true;
        bool removed = false;
        Entity* owner = nullptr;

    public:
        
        void Serialize(XMLNode &node) const override
        {
            XMLNode component = node.AddChild("Component");
            component.SetAttribute("started", started);
            component.SetAttribute("enabled", enabled);
            component.SetAttribute("removed", removed);
            // owner is runtime only
        }

        void Deserialize(const XMLNode &node) override
        {
            XMLNode component = node.GetChild("Component");
            component.GetAttribute("started", started);
            component.GetAttribute("enabled", enabled);
            component.GetAttribute("removed", removed);
        }
    };
}