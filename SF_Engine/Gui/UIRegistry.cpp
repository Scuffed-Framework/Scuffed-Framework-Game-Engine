#include "UIRegistry.hpp"
#include <Gui/ocornut/imgui.h>

namespace SF::Engine
{
    // Register a raw draw function

    // Remove a previously registered window or function by handle
    void UIRegistry::Unregister(Handle handle)
    {
        std::erase_if(drawFns_, [handle](const auto &entry)
                      { return entry.first == handle; });

        // Also clean up the type-dedup map if this was a typed registration
        std::erase_if(typeHandles_, [handle](const auto &entry)
                      { return entry.second == handle; });
    }

    void UIRegistry::DrawAll()
    {
        for (auto &[handle, fn] : drawFns_)
            fn();
    }
}