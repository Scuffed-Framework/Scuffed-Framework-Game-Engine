#pragma once
#include <Commands/CommandsWindow.hpp>
#include <Engine/Application/Application.hpp>
#include "Panels/AssetsWindow.hpp"
#include "Panels/BarPanels.hpp"
#include "Panels/HierarchyPanel.hpp"
#include "Panels/InspectorPanel.hpp"

namespace SF::Engine
{
    class EditorInfo : public ::SF::Engine::AppInfo
    {
    public:
        EditorInfo() : AppInfo{/*Name*/ {"SF Level Editor"}, /*Version*/ {1, 0, 0}} {};
    };

    class EditorApplication : public Application
    {
    public:
        EditorApplication() :
            Application(std::make_unique<EditorInfo>()), hierarchy(std::make_unique<HierarchyPanel>()),
            inspector(std::make_unique<InspectorPanel>()), panels(std::make_unique<BarPanels>()),
            assetBrowser(std::make_unique<AssetBrowser>()), cmdWindow(std::make_unique<CommandWindow>())
        {
        }

        InitializationReturn Init() override
        {

            if (const auto result = Application::Init(); result != InitializationReturn::Success)
                return result;

            hierarchy->SetOnEntitySelected([this](Entity *entity) { inspector->SetEntity(entity); });

            return InitializationReturn::Success;
        }

        void Update() override { Application::Update(); }

    private:
        std::unique_ptr<HierarchyPanel> hierarchy;
        std::unique_ptr<InspectorPanel> inspector;
        std::unique_ptr<BarPanels> panels;
        std::unique_ptr<AssetBrowser> assetBrowser;
        std::unique_ptr<CommandWindow> cmdWindow;
    };
} // namespace SF::Engine
