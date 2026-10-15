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
#include "../Tests/Audio/AudioTests.hpp"

namespace SF::Engine
{
    class Camera;
    class AudioClip;
    class SoundBuffer;


    class BarPanels : public StaticSingleInstancePanel<BarPanels> // (and tests)
    {
    public:
        BarPanels()
        {
            reg        = UIRegistry::Get().Register([this] { Draw(); });
            audioTests = std::make_unique<AudioTests>();
        };
        ~BarPanels() { UIRegistry::Get().Unregister(reg); };

        void Draw();

    private:
        size_t reg;
        void TestAudio(uint32_t dat, float freq, float time);
        std::shared_ptr<SoundBuffer> buffer;
        std::shared_ptr<AudioClip> clip;

        void DrawMenuBar();
        void DrawAudioTools();

        void DrawEngineStatusBar();
        void DrawExecutingPasses();

        // tests:
        std::unique_ptr<AudioTests> audioTests;
    };
} // namespace SF::Engine
