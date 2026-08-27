#pragma once

#include <Communication/Events/EventDispatcher.hpp>

namespace SF::Engine
{
    class Layer
    {
    public:
        virtual ~Layer() = default;

        Layer(const Layer&) = delete;
        Layer& operator=(const Layer&) = delete;
        Layer(Layer&&) = default;
        Layer& operator=(Layer&&) = default;

        virtual void OnAttach() {}
        virtual void OnDetach() {}
        virtual void OnUpdate() {}
        virtual void OnEvent(Event& event) {}  // Generic event handler
        virtual void OnImGuiRender() {}

    protected:
        Layer() = default;
    };
}  // namespace SF::Engine