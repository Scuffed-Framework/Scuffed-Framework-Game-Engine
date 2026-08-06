#pragma once
#include <Graphics/Windows/WindowManager.hpp>
#include <Graphics/Images/Imaged2d>
#include <Video/Video.hpp>
#include <unordered_map>
#include <Engine/Init/SplashScreenQuotes.hpp>
#include <TemplateLibrary/Types.hpp>
#include "EngineSplashScreen.hpp"

namespace SF::Engine
{
    namespace Internal
    {
        /*create image2d from bitmap
        Bitmap(std::unique_ptr<uint8_t[]> &&data, const UVec2 &size,
               uint32_t bytesPerPixel = 4);

        embed image data from SF::Engine::Internal::MagickImage (uint8[])
        */
       
    }
    // Editor & Engine have their splashscreens, Editor is outside of a window as a floating image, like Unreal or Unity
    // Engine is in the window, a video/gif, and more images, credits

    class SplashScreenRenderer
    {
    public:
        void Init();
        virtual void Show();
        void Stop();
        void AddImage(Image2d, float);

        std::unordered_map<Image2d /*img*/, float/*time*/> Images_;
        Window SplashScreenWindow_;
    };
}