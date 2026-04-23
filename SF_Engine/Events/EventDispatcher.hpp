#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace SF::Engine
{
    // Base event class - all events inherit from this
    struct Event
    {
        virtual ~Event() = default;
        bool handled = false;
    };

    // Event listener handle for unsubscribing
    class EventHandle
    {
        friend class EventDispatcher;

        std::type_index _type;
        size_t _id;

        EventHandle(std::type_index type, size_t id) : _type(type), _id(id) {}

    public:
        EventHandle() : _type(typeid(void)), _id(0) {}

        bool IsValid() const
        {
            return _id != 0;
        }
    };

    // Event dispatcher - manages all event subscriptions and dispatching
    class EventDispatcher
    {
    private:
        struct ListenerBase
        {
            virtual ~ListenerBase() = default;
            size_t id;
            virtual void Invoke(Event&) = 0;
        };

        template <typename T>
        struct Listener : ListenerBase
        {
            std::function<void(T&)> callback;

            Listener(size_t id, std::function<void(T&)> cb) : callback(std::move(cb))
            {
                this->id = id;
            }

            void Invoke(Event& e) override
            {
                callback(static_cast<T&>(e));
            }
        };

        std::unordered_map<std::type_index, std::vector<std::unique_ptr<ListenerBase>>> _listeners;
        size_t _nextId = 1;

    public:
        // Subscribe to an event type
        template <typename EventType>
        EventHandle Subscribe(std::function<void(EventType&)> callback)
        {
            static_assert(std::is_base_of_v<Event, EventType>, "EventType must derive from Event");

            auto& listeners = _listeners[typeid(EventType)];
            size_t id = _nextId++;

            listeners.push_back(std::make_unique<Listener<EventType>>(id, std::move(callback)));

            return EventHandle(typeid(EventType), id);
        }

        // Convenience overload for member functions
        template <typename EventType, typename T>
        EventHandle Subscribe(T* instance, void (T::*method)(EventType&))
        {
            return Subscribe<EventType>([instance, method](EventType& e)
                                        { (instance->*method)(e); });
        }

        // Unsubscribe using handle
        void Unsubscribe(EventHandle handle)
        {
            if (!handle.IsValid()) return;

            auto it = _listeners.find(handle._type);
            if (it == _listeners.end()) return;

            auto& listeners = it->second;
            listeners.erase(std::remove_if(listeners.begin(), listeners.end(),
                                           [id = handle._id](const auto& listener)
                                           { return listener->id == id; }),
                            listeners.end());
        }

        // Dispatch an event to all subscribers
        template <typename EventType>
        void Dispatch(EventType& event)
        {
            static_assert(std::is_base_of_v<Event, EventType>, "EventType must derive from Event");

            auto it = _listeners.find(typeid(EventType));
            if (it == _listeners.end()) return;

            for (auto& listenerBase : it->second)
            {
                if (event.handled) break;

                auto* listener = static_cast<Listener<EventType>*>(listenerBase.get());
                listener->callback(event);
            }
        }

        // Dispatch by creating event inline
        template <typename EventType, typename... Args>
        void Dispatch(Args&&... args)
        {
            EventType event(std::forward<Args>(args)...);
            Dispatch(event);
        }

        // Clear all listeners for a specific event type
        template <typename EventType>
        void ClearListeners()
        {
            _listeners.erase(typeid(EventType));
        }

        // Clear all listeners
        void ClearAll()
        {
            _listeners.clear();
        }
    };

    // RAII-style scoped event subscription?
    class ScopedEventHandle
    {
        EventDispatcher* _dispatcher = nullptr;
        EventHandle _handle;

    public:
        ScopedEventHandle() = default;

        ScopedEventHandle(EventDispatcher* dispatcher, EventHandle handle)
            : _dispatcher(dispatcher), _handle(handle)
        {
        }

        ~ScopedEventHandle()
        {
            if (_dispatcher && _handle.IsValid())
            {
                _dispatcher->Unsubscribe(_handle);
            }
        }

        // Non-copyable
        ScopedEventHandle(const ScopedEventHandle&) = delete;
        ScopedEventHandle& operator=(const ScopedEventHandle&) = delete;

        // Movable
        ScopedEventHandle(ScopedEventHandle&& other) noexcept
            : _dispatcher(other._dispatcher), _handle(other._handle)
        {
            other._dispatcher = nullptr;
            other._handle = EventHandle();
        }

        ScopedEventHandle& operator=(ScopedEventHandle&& other) noexcept
        {
            if (this != &other)
            {
                if (_dispatcher && _handle.IsValid())
                {
                    _dispatcher->Unsubscribe(_handle);
                }
                _dispatcher = other._dispatcher;
                _handle = other._handle;
                other._dispatcher = nullptr;
                other._handle = EventHandle();
            }
            return *this;
        }

        void Unsubscribe()
        {
            if (_dispatcher && _handle.IsValid())
            {
                _dispatcher->Unsubscribe(_handle);
                _handle = EventHandle();
            }
        }
    };

}  // namespace SF::Engine