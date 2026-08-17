#pragma once
#include <Rendering/Images/ImageAsset.hpp>
#include <Gui/UIRegistry.hpp>
#include <Gui/ocornut/imgui.h>
#include <Gui/ocornut/imgui_internal.h>
#include <Project/Project.hpp>
#include <Gui/IconHeaders/IconMaterialDesign.hpp>
#include <algorithm>
#include <cctype>
#include <unordered_map>
#include <Assets/AssetPipeline.hpp>
#include <Reflection/RTTI/RTTICast.hpp>

namespace SF::Engine
{
    class AssetBrowser
    {
    public:
        enum class ViewMode
        {
            Grid,
            List,
            Details
        };

        AssetBrowser()
        {
            m_uiHandle = UIRegistry::Get().Register([this]
                                                    { Draw(); });
        }

        void Draw();

    private:
        std::size_t m_uiHandle;
        std::filesystem::path m_currentPath;
        ViewMode m_viewMode = ViewMode::Grid;
        std::string m_searchFilter;
        AssetType m_typeFilter = static_cast<AssetType>(-1); // -1 = All
        std::optional<UUID> m_selectedAsset;
        float m_thumbnailSize = 80.0f;
        bool m_showThumbnails = true;

        // Cache for texture previews
        struct TexturePreview
        {
            ImTextureID textureID = 0; // Null
            UVec2 size;
            bool isValid = false;
        };
        std::unordered_map<UUID, TexturePreview> m_previewCache;
        template <typename Func>
        bool TryWithImageTexture(const std::shared_ptr<AssetBase> &asset, Func &&fn);

        void DrawMenuBar();
        void DrawToolbar();
        void DrawPathNavigation();
        void DrawFilterControls();
        void DrawAssetGrid();
        void DrawGridView(const SFTL::DynamicArray<std::shared_ptr<AssetBase>> &assets);
        void DrawAssetTile(const std::shared_ptr<AssetBase> &asset, int index);
        void DrawListView(const SFTL::DynamicArray<std::shared_ptr<AssetBase>> &assets);
        void DrawSmallPreview(const std::shared_ptr<AssetBase> &asset);
        void DrawDetailsPanel();
        void DrawGenericAssetDetails(const std::shared_ptr<AssetBase> &asset);
        void AddPropertyRow(const std::string &label, const std::string &value);

        template <typename TImage>
        void DrawImageDetails(const std::shared_ptr<ImageAsset<TImage>> &asset);

        template <typename T>
        void DrawActionButtons(const std::shared_ptr<T> &asset);
        void DrawAssetIcon(const std::shared_ptr<AssetBase> &asset);
        const char *GetAssetTypeName(AssetType type);
        const char *GetFormatName(VkFormat format);
        const char *GetFilterName(VkFilter filter);
        const char *GetAddressModeName(VkSamplerAddressMode mode);
        bool ShouldShowAsset(const std::shared_ptr<AssetBase> &asset);

        template <typename TImage>
        void CreateImageAsset();
        void RefreshAssets();

        // Helper to get or create texture preview
        template <typename TImage>
        TexturePreview GetOrCreatePreview(const UUID &guid, const std::shared_ptr<TImage> &texture);
    };
}