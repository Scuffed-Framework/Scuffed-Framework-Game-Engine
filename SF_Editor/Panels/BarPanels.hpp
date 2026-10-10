#pragma once
#include <Gui/ImGui/ocornut/imgui.h>
#include <Rendering/Camera/EditorCamera.hpp>
#include <cstdio>
#include <string>
#include <vector>

#include <Configuration/Default/ImGuiDefaultWidgets.hpp>
#include <Scene/Types.hpp>

#include <Commands/CommandsWindow.hpp>

#include <Gui/ImGui/StaticPanel.hpp>
#include <Gui/ImGui/ocornut/imgui_stdlib.h>

namespace SF::Engine
{
    class Camera;
    class AudioClip;
    class SoundBuffer;

    class BarPanels : public StaticSingleInstancePanel<BarPanels>
    {
    public:
        BarPanels()
        {
            reg = UIRegistry::Get().Register([this] { Draw(); });
        };
        ~BarPanels() { UIRegistry::Get().Unregister(reg); };

        void Draw();

    private:
        size_t reg;
        void TestAudio(uint32_t dat, float freq, float time);
        std::shared_ptr<SoundBuffer> buffer;
        std::shared_ptr<AudioClip> clip;

        void DrawMenuBar();

        void DrawObjectNode(EntityId entityId);
        void DrawFolderNode(EntityId entityId);
        void AddFolder(const std::string &name, Entity *parent);

        void DrawEngineStatusBar();
        void DrawExecutingPasses();
    };
} // namespace SF::Engine
