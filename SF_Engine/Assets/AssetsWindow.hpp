#pragma once
#include "AssetPipeline.hpp"
#include <Gui/UIRegistry.hpp>
#include <Gui/ocornut/imgui.h>

namespace SF::Engine
{
    class AssetBrowser
    {
        std::size_t m_uiHandle;

    public:
        AssetBrowser()
        {
            m_uiHandle = UIRegistry::Get().Register([this]
                                                    { Draw(); });
        }

        void Draw()
        {
            ImGui::Begin("Asset Browser");
            // render folders and whatnot
            // TODO: add a way to force imgui to have certain glyphs from another font
            ImGui::End();
        }
    };

}