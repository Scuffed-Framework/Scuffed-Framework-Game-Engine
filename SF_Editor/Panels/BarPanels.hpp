#pragma once
#include <Gui/ocornut/imgui.h>
#include <Rendering/Camera/EditorCamera.hpp>
#include <cstdio>
#include <string>
#include <vector>

#include <Configuration/Default/ImGuiDefaultWIDGETS.hpp>
#include <Scene/Types.hpp>

#include <Commands/CommandsWindow.hpp>

#include <1stPartyLibs/TemplateLibrary/Types.hpp>
#include <Gui/StaticPanel.hpp>
#include <Gui/ocornut/imgui_stdlib.h>

namespace SF::Engine
{
    class Camera;

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
        ::SFTL::size_type reg;

        void DrawMenuBar();

        void DrawObjectNode(EntityId entityId);
        void DrawFolderNode(EntityId entityId);
        void AddFolder(const std::string &name, Entity *parent);

        void DrawEngineStatusBar();
        void DrawExecutingPasses();
    };
} // namespace SF::Engine
