#pragma once
#include <Gui/ocornut/imgui.h>
#include <string>
#include <vector>
#include <Camera/EditorCamera.hpp>
#include <cstdio>

#include <Scene/Types.hpp>
#include <Default/ImGuiDefaultWIDGETS.hpp>

#include <Commands/CommandsWindow.hpp>

#include <Gui/ocornut/imgui_stdlib.h>
#include <TemplateLibrary/Types.hpp>
#include <Gui/StaticPanel.hpp>

namespace SF::Engine
{
    class Camera;

    class BarPanels : public StaticSingleInstancePanel<BarPanels>
    {
    public:
        BarPanels(){ reg = UIRegistry::Get().Register([this]{Draw();});};
        ~BarPanels() {UIRegistry::Get().Unregister(reg);};

        void Draw();

    private:
        ::SFTL::size_type reg;

        void DrawMenuBar();

        void DrawObjectNode(EntityId entityId);
        void DrawFolderNode(EntityId entityId);
        void AddFolder(const std::string &name, Entity *parent);

        void DrawEngineStatusBar();
    };
}