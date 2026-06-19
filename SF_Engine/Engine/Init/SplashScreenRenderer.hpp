#include <Graphics/Windows/Windows.hpp>

namespace SF::Engine
{
    class SplashScreenRenderer
    {
        void PreInit()
        {
            glfwHideWindow(WindowManager::Get()->GetWindow(0));
        }
        void Init()
        {
            // show
        }
        void PostInit()
        {
            // init done, show window
            glfwShowWindow(WindowManager::Get()->GetWindow(0));
        }
    };
}