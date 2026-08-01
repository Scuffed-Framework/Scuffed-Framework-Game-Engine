#pragma once
#include <Graphics/Windows/WindowManager.hpp>
#include <Graphics/Images/Imaged2d>
#include <Video/Video.hpp>
#include <vector>
#include "SplashScreenQuotes.hpp"

namespace SF::Engine
{
    // Editor & Engine have their splashscreens, Editor is outside of a window as a floating image, like Unreal or Unity
    // Engine is in the window, a video/gif, and more images, credits

    class SplashScreenRenderer
    {
    public:
        void Init();
        virtual void Show();
        void Stop();

        std::vector<Image2d> Images_;
        Window SplashScreenWindow_;
    };
}