#pragma once
#include <Assets/AssetPipeline.hpp>
#include <Assets/AssetTextures.hpp>
#include <Gui/ImGui/IconHeaders/IconMaterialDesign.hpp>
#include <Gui/ImGui/UIRegistry.hpp>
#include <Gui/ImGui/ocornut/imgui.h>
#include <Gui/ImGui/ocornut/imgui_internal.h>
#include <LowLevel/Rocket.hpp>
#include <LowLevel/Reflection/RTTI/RTTICast.hpp>
#include <Rendering/RHI/Images/Image.hpp>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// X.h leaks this
#ifdef None
    #undef None
#endif

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
            s_instance = this;
            m_uiHandle = UIRegistry::Get().Register([this] { Draw(); });
        }
        ~AssetBrowser()
        {
            m_dropConnection.disconnect();
            if (s_instance == this)
                s_instance = nullptr;
        }
        // The UI registry holds a lambda capturing `this`, so copies would dangle.
        AssetBrowser(const AssetBrowser &)            = delete;
        AssetBrowser &operator=(const AssetBrowser &) = delete;

        void Draw();
        void BeginNewScene();

        static AssetBrowser *Get() { return s_instance; }

    private:
        using AssetList = vector<std::shared_ptr<AssetBase>>;

        enum class InlineEditMode
        {
            None,
            RenameFolder,
            NewScene,
            RenameScene
        };

        struct TexturePreview
        {
            ImTextureID textureID = 0; // null
            UVec2 size{};
            bool isValid = false;
        };

        struct DirEntry
        {
            std::filesystem::path path;
            std::string name; // display name; scene files have their extension stripped
        };

        // TODO: Replace with Assets/Bitmaps/SVG. Only `folder` and `scene` are used right now.
        struct Icons
        {
            std::shared_ptr<Image2d> save, newFile, hpp, h, folder, file, c, cpp, scene;
        };

        static constexpr AssetType kAllTypes = static_cast<AssetType>(-1);

        // ---- layout
        void EnsureIconsLoaded();
        void DrawContent();
        void DrawMenuBar();
        void DrawCreateAssetItems();
        void DrawToolbar();
        void DrawPathNavigation();
        void DrawFilterControls();
        void DrawAssetGrid();
        void DrawContextMenu();

        //  navigation
        void NavigateTo(const std::filesystem::path &path);
        void NavigateUp();
        void RefreshAssets();
        void RefreshDirectoryCache();
        std::filesystem::path CreateUniqueFolder(const std::filesystem::path &parent, const std::string &baseName);
        void OpenSceneFile(const std::filesystem::path &file);

        //  import / placement
        void EnsureDropSubscribed();
        void ProcessPendingDrops();   // OS files dropped on the editor window (queued by the GLFW callback)
        void ProcessSceneAssetDrag(); // an asset tile dragged out of this panel and released over the 3D view
        void ImportFiles(const std::vector<std::filesystem::path> &files, bool placeInScene);
        bool GroundPointUnderCursor(Vec3 &out) const;
        void SetStatus(std::string text, bool isError);
        void DrawStatus();

        //  grid view
        void DrawGridView(const AssetList &assets);
        // Returns true when the tile was double-clicked.
        bool DrawIconTile(const std::shared_ptr<Image2d> &icon, const std::string &name, bool editing,
                          bool highlighted = false);
        void DrawFolderTile(const DirEntry &folder);
        void DrawSceneFileTile(const DirEntry &scene);
        void DrawNewSceneTile();
        void DrawAssetTile(const std::shared_ptr<AssetBase> &asset, int index);
        void DrawAssetThumbnail(const std::shared_ptr<AssetBase> &asset, Vec2 size);
        void DrawPlaceholder(const std::shared_ptr<AssetBase> &asset, const char *id, Vec2 size, const Vec4 &color);
        bool TryDrawThumbnail(const std::shared_ptr<AssetBase> &asset, Vec2 box);
        void DrawFolderContextMenu(const DirEntry &folder);
        void DrawSceneContextMenu(const DirEntry &scene);
        void DrawAssetContextMenu(const std::shared_ptr<AssetBase> &asset);

        // - list view
        void DrawListView(const AssetList &assets);
        void DrawFolderRow(const DirEntry &folder);
        void DrawNewSceneRow();
        void DrawSceneRow(const DirEntry &scene);
        void DrawAssetRow(const std::shared_ptr<AssetBase> &asset, int index);
        void DrawSmallPreview(const std::shared_ptr<AssetBase> &asset);

        //  details panel
        void DrawDetailsPanel();
        void DrawGenericAssetDetails(const std::shared_ptr<AssetBase> &asset);

        template<typename TImage>
        void DrawImageDetails(const std::shared_ptr<ImageAsset<TImage>> &asset);

        template<typename T>
        void DrawActionButtons(const std::shared_ptr<T> &asset);

        //  assets
        bool ShouldShowAsset(const std::shared_ptr<AssetBase> &asset) const;

        template<typename TImage>
        void CreateImageAsset();

        template<typename TImage>
        const TexturePreview &GetOrCreatePreview(const UUID &guid, const std::shared_ptr<TImage> &texture);

        //  inline edit
        void BeginInlineEdit(InlineEditMode mode, const std::filesystem::path &path, const std::string &text);
        void EndInlineEdit();
        bool IsEditing(InlineEditMode mode, const std::filesystem::path &path) const;
        bool IsCreatingSceneHere() const;
        void DrawInlineEditField(float width);
        void CommitInlineEdit();
        void CommitNewScene();
        void CommitFolderRename();
        void CommitSceneRename();
        void BeginRenameFolder(const std::filesystem::path &path);
        void BeginRenameScene(const std::filesystem::path &file);

        //  delete
        void RequestDelete(const std::filesystem::path &path, bool isScene);
        void DrawDeleteConfirmPopup();
        void ExecuteDelete();

        //  state
        std::size_t m_uiHandle;
        Icons m_icons;

        // navigation / view
        std::filesystem::path m_currentPath;
        std::string m_currentPathText; // m_currentPath.string(), kept in sync by NavigateTo
        ViewMode m_viewMode   = ViewMode::Grid;
        float m_thumbnailSize = 80.0f;
        bool m_showThumbnails = true;
        bool m_detailsOpen    = false;

        // filtering
        char m_searchBuffer[256] = {};
        std::string m_searchFilter; // lower-cased copy of m_searchBuffer
        AssetType m_typeFilter = kAllTypes;

        // selection + previews
        std::optional<UUID> m_selectedAsset;
        std::unordered_map<UUID, TexturePreview> m_previewCache;

        // directory cache: rebuilt on navigation, on mutation (m_dirDirty) or when the folder's mtime changes
        std::filesystem::path m_cachedPath;
        std::filesystem::file_time_type m_cachedStamp{};
        double m_nextDirPoll = 0.0;
        bool m_dirDirty      = true;
        std::vector<DirEntry> m_folders;
        std::vector<DirEntry> m_sceneFiles;

        // inline rename / new-scene field
        InlineEditMode m_inlineEditMode = InlineEditMode::None;
        std::filesystem::path m_inlineEditPath;
        char m_inlineEditBuffer[256]  = {};
        bool m_inlineEditFocusPending = false;

        // delete confirmation
        bool m_showDeleteConfirmPopup = false;
        bool m_deleteTargetIsScene    = false;
        std::filesystem::path m_deleteTargetPath;

        // import / placement state
        rocket::connection m_dropConnection;
        bool m_dropSubscribed = false;
        std::vector<std::filesystem::path> m_pendingDrops;
        Vec2 m_windowMin{};
        Vec2 m_windowMax{};
        bool m_windowRectValid = false;
        std::optional<UUID> m_dragAsset; // asset tile currently being dragged
        bool m_dragConsumed = false;     // drop already handled while ImGui's payload lingers
        std::string m_status;
        bool m_statusIsError = false;
        double m_statusUntil = 0.0;

        inline static AssetBrowser *s_instance = nullptr;
    };
} // namespace SF::Engine
