#include <Engine/Engine.hpp>
#include "Application/LevelEditor.hpp"

int main()
{
    // TODO: engine should create the editor and loads the main scene
    SF::Engine::EditorApplication app;

    if (auto result = app.Init(); result != SF::Engine::EditorApplication::Success)
        return -1;

    app.AppLoop();

    return 0;
}
