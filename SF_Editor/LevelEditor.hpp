#pragma once
#include <Application/Application.hpp>

namespace SF::Editor
{
    class EditorInfo : public ::SF::Engine::GameInfo
    {
        EditorInfo() : GameInfo{/*Name*/ {"SF Level Editor"}, /*Version*/ {1, 0, 0}} {};
    };
    class EditorApplication : public ::SF::Engine::Application<EditorInfo>
    {
    public:
        InitializationReturn Init() override
        {
            Application::Init();
        }
        void Update() override
        {
            Application::Update();
        }
    };
}