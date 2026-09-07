#pragma once

#include <Engine/Engine.hpp>
#include "Window.hpp"

namespace SF::Engine
{
    /**
     * @brief Module used for managing a window.
     */
    class WindowManager : public ModuleRegistrar<WindowManager>
    {
        REGISTER_MODULE(WindowManager, ModuleStage::Pre);

    public:
        WindowManager();
        ~WindowManager();

        void Update() override;

        Window *AddWindow();
        const Window *GetWindow(WindowId id) const;
        Window *GetWindow(WindowId id);

        const std::vector<std::unique_ptr<Monitor>> &GetMonitors() const
        {
            return monitors;
        };

        const Monitor *GetPrimaryMonitor() const;

        /**
         * Called when a window has been added or closed.
         * @return The rocket::signal.
         */
        rocket::signal<void(Window *, bool)> &OnAddWindow()
        {
            return onAddWindow;
        }

        /**
         * Called when a monitor has been connected or disconnected.
         * @return The rocket::signal.
         */
        rocket::signal<void(Monitor *, bool)> &OnMonitorConnect()
        {
            return onMonitorConnect;
        }

        static void CheckGlfw(int32_t result);

        std::pair<const char **, uint32_t> GetInstanceExtensions() const;

        std::vector<std::unique_ptr<Window>>& GetWindows() { return windows; }

        std::vector<std::unique_ptr<Monitor>>& GetMonitors() { return monitors; }

    private:
        friend void CallbackError(int32_t error, const char *description);
        friend void CallbackMonitor(GLFWmonitor *glfwMonitor, int32_t event);

        std::vector<std::unique_ptr<Window>> windows;

        std::vector<std::unique_ptr<Monitor>> monitors;

        rocket::signal<void(Window *, bool)> onAddWindow;
        rocket::signal<void(Monitor *, bool)> onMonitorConnect;
    };
}