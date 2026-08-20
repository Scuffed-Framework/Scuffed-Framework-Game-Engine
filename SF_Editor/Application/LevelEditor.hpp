#pragma once
#include <Application/Application.hpp>
#include "Panels/HierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/BarPanels.hpp"
#include "Panels/AssetsWindow.hpp"
#include <Commands/CommandsWindow.hpp>

namespace SF::Engine
{
    class EditorInfo : public ::SF::Engine::GameInfo
    {
    public:
        EditorInfo() : GameInfo{/*Name*/ {"SF Level Editor"}, /*Version*/ {1, 0, 0}} {};
    };

    class EditorApplication : public Application<EditorInfo>
    {
    public:
        EditorApplication()
            : Application(EditorInfo{}),
              hierarchy(std::make_unique<HierarchyPanel>()),
              inspector(std::make_unique<InspectorPanel>()),
              panels(std::make_unique<BarPanels>()),
              assetBrowser(std::make_unique<AssetBrowser>()),
              cmdWindow(std::make_unique<CommandWindow>())
        {
        }

        InitializationReturn Init() override
        {
            const auto result = Application::Init();

            if (result != InitializationReturn::Success)
                return result;

            hierarchy->SetOnEntitySelected(
                [this](Entity *entity)
                {
                    inspector->SetEntity(entity);
                });

            return InitializationReturn::Success;
        }

        void Update() override
        {
            Application::Update();
        }

    private:
        std::unique_ptr<HierarchyPanel> hierarchy;
        std::unique_ptr<InspectorPanel> inspector;
        std::unique_ptr<BarPanels> panels;
        std::unique_ptr<AssetBrowser> assetBrowser;
        std::unique_ptr<CommandWindow> cmdWindow;
    };
}