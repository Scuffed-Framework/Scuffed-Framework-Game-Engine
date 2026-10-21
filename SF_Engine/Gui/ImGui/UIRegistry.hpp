#pragma once
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <UtilityClasses/RegistryBase.hpp>
#include <concepts>

struct ImGuiWindow;

namespace SF::Engine
{
    class UIRegistry : public Registry<UIRegistry>
    {
        friend class Registry<UIRegistry>; // allow CRTP base to call private ctor

    public:
        using DrawFn = std::function<void()>;
        using Handle = std::size_t;

        // Register a window class; safe against duplicate calls
        template <typename T>
        Handle Register()
        {
            static_assert(std::derived_from<T, ::ImGuiWindow>, "T must derive from ImGuiWindow");

            // Deduplicate by type: same T always returns the same handle
            auto id = std::type_index(typeid(T));
            if (auto it = typeHandles_.find(id); it != typeHandles_.end())
                return it->second;

            auto window = std::make_shared<T>();
            Handle handle = Register([window]()
                                     { window->Draw(); });
            typeHandles_.emplace(id, handle);
            return handle;
        }

        // Register a raw draw function
        Handle Register(DrawFn fn)
        {
            Handle handle = nextHandle_++;
            drawFns_.emplace_back(handle, std::move(fn));
            return handle;
        }

        // Remove a previously registered window or function by handle
        void Unregister(Handle handle);

        void DrawAll();

    private:
        UIRegistry() = default;

        std::vector<std::pair<Handle, DrawFn>> drawFns_;
        std::unordered_map<std::type_index, Handle> typeHandles_; // dedup typed registrations
        Handle nextHandle_ = 0;
    };
}