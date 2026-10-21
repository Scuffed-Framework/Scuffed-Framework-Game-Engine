#include "AssetsWindow.hpp"
#include <Engine/Project/Project.hpp>
#include <Gui/ImGui/CommonGuiFunctions.hpp>
#include <Gui/ImGui/ocornut/imgui_impl_vulkan.h>
#include <LowLevel/FileSystem/File.hpp>
#include <Scene/Scene.hpp>
#include "../Wizards/Shaders.hpp"
#include "Panels.hpp"
#include "Scene/SceneManager.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <system_error>

namespace SF::Engine
{
    namespace
    {
        constexpr const char *kTypeFilterNames[] = {"All",
                                                    "Mesh",
                                                    "Texture",
                                                    "Shader",
                                                    "LuaScript",
                                                    "CppCode",
                                                    "VFX",
                                                    "Audio",
                                                    "SkeletalAnimation",
                                                    "Scene",
                                                    "TemplateAsset",
                                                    "ConfigFile",
                                                    "Font",
                                                    "Material",
                                                    "Library",
                                                    "AnimationStateMachine"};

        constexpr char kWindowTitle[] = ICON_MD_FOLDER " Asset Browser";

        constexpr double kDirPollIntervalSec      = 0.5;
        constexpr float kTileLabelPadding         = 4.0f;
        constexpr float kListPreviewSize          = 50.0f;
        constexpr float kDetailsPreviewMaxHeight  = 300.0f;
        constexpr std::uint32_t kDefaultImageSize = 256;
        constexpr VkFormat kDefaultImageFormat    = VK_FORMAT_R8G8B8A8_UNORM;

        constexpr Vec4 kHeadingColor(0.5f, 0.8f, 1.0f, 1.0f);
        constexpr Vec4 kErrorColor(1.0f, 0.3f, 0.3f, 1.0f);
        constexpr Vec4 kWarningColor(1.0f, 0.6f, 0.2f, 1.0f);
        constexpr Vec4 kCurrentSceneColor(0.45f, 0.80f, 1.0f, 1.0f);

        struct ViewModeInfo
        {
            AssetBrowser::ViewMode mode;
            const char *icon;
            const char *menuLabel;
        };
        constexpr ViewModeInfo kViewModes[] = {
                {AssetBrowser::ViewMode::Grid, ICON_MD_GRID_ON, ICON_MD_GRID_ON " Grid"},
                {AssetBrowser::ViewMode::List, ICON_MD_FORMAT_LIST_BULLETED, ICON_MD_FORMAT_LIST_BULLETED " List"},
                {AssetBrowser::ViewMode::Details, ICON_MD_DETAILS, ICON_MD_DETAILS " Details"}};

        // Pushes a two-level ID ("folder" + "Textures") so a folder and a scene file
        // that share a name can never collide.
        struct ScopedId
        {
            ScopedId(const char *scope, const std::string &name)
            {
                ImGui::PushID(scope);
                ImGui::PushID(name.c_str());
            }
            ~ScopedId()
            {
                ImGui::PopID();
                ImGui::PopID();
            }
            ScopedId(const ScopedId &)            = delete;
            ScopedId &operator=(const ScopedId &) = delete;
        };

        std::string ToLower(std::string_view s)
        {
            std::string out(s);
            ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return out;
        }

        bool ContainsNoCase(std::string_view haystack, std::string_view needle)
        {
            if (needle.empty())
                return true;
            const auto it =
                    ranges::search(
                            haystack, needle, [](char h, char n)
                            { return std::tolower(static_cast<unsigned char>(h)) == static_cast<unsigned char>(n); })
                            .begin();
            return it != haystack.end();
        }

        bool LessNoCase(const std::string &a, const std::string &b)
        {
            return ranges::lexicographical_compare(a, b, [](unsigned char x, unsigned char y)
                                                   { return std::tolower(x) < std::tolower(y); });
        }

        std::string CleanFileName(std::string s)
        {
            for (char &c: s)
                if (std::strchr("<>:\"/\\|?*", c) || static_cast<unsigned char>(c) < 32)
                    c = '_';
            s.erase(s.find_last_not_of(" \t\n\r\f\v") + 1); // trailing whitespace (npos + 1 == 0 -> clears all-blank)
            return s;
        }

        bool IsSceneFile(const std::filesystem::path &p)
        {
            const std::string f        = p.filename().string();
            const std::string_view ext = SceneFileExtension;
            return f.size() > ext.size() && f.compare(f.size() - ext.size(), ext.size(), ext) == 0;
        }

        // "Level1.scene" -> "Level1". Only call on paths that passed IsSceneFile.
        std::string SceneDisplayName(const std::filesystem::path &p)
        {
            std::string name = p.filename().string();
            name.resize(name.size() - SceneFileExtension.size());
            return name;
        }

        // True if `path` is `folder` or lives somewhere below it (component-wise, not a string prefix).
        bool IsSameOrInside(const std::filesystem::path &path, const std::filesystem::path &folder)
        {
            const auto rel = path.lexically_relative(folder);
            return !rel.empty() && *rel.begin() != std::filesystem::path("..");
        }

        std::string ToHex(std::uint32_t v)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "0x%X", v);
            return buf;
        }

        // Scale `src` to fit inside `box`, preserving aspect ratio.
        Vec2 FitInside(Vec2 src, Vec2 box, bool allowUpscale)
        {
            if (src.x <= 0.0f || src.y <= 0.0f)
                return box;
            float scale = std::min(box.x / src.x, box.y / src.y);
            if (!allowUpscale)
                scale = std::min(scale, 1.0f);
            return {src.x * scale, src.y * scale};
        }

        void ToolbarGap()
        {
            ImGui::SameLine();
            ImGui::Dummy(Vec2(10, 0));
            ImGui::SameLine();
        }

        void Heading(const char *text) { ImGui::TextColored(kHeadingColor, "%s", text); }

        void DrawIcon(const std::shared_ptr<Image2d> &icon, Vec2 size)
        {
            if (icon)
                ImGui::Image(icon->GetTexID(), size);
            else
                ImGui::Dummy(size);
        }

        // Outline around the last item (used for the selected tile).
        void DrawSelectionOutline()
        {
            ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                ImGui::GetColorU32(ImGuiCol_HeaderActive), 3.0f);
        }

        bool BeginPropertyTable(const char *id)
        {
            if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                return false;
            ImGui::TableSetupColumn(ICON_MD_LABEL " Property", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();
            return true;
        }

        void PropertyRow(const char *label, std::string_view value)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value.data(), value.data() + value.size());
        }


        template<typename TImage, typename Fn>
        bool TryVisit(const std::shared_ptr<AssetBase> &asset, Fn &fn)
        {
            if (const auto image = ::SF::RTTI::rtti_pointer_cast<ImageAsset<TImage>>(asset))
            {
                fn(image);
                return true;
            }
            return false;
        }

        template<typename Fn>
        bool VisitImageAsset(const std::shared_ptr<AssetBase> &asset, Fn &&fn)
        {
            return TryVisit<Image2d>(asset, fn) || TryVisit<Image3d>(asset, fn) || TryVisit<Image2dArray>(asset, fn) ||
                   TryVisit<Cubemap>(asset, fn);
        }

        const char *GetAssetIcon(AssetType type)
        {
            switch (type)
            {
                case AssetType::Texture:
                    return ICON_MD_IMAGE;
                case AssetType::Mesh:
                    return ICON_MD_3D_ROTATION;
                case AssetType::Shader:
                    return ICON_MD_CODE;
                case AssetType::Material:
                    return ICON_MD_PALETTE;
                case AssetType::Audio:
                    return ICON_MD_AUDIO_FILE;
                case AssetType::Scene:
                    return ICON_MD_WEB;
                case AssetType::Font:
                    return ICON_MD_FONT_DOWNLOAD;
                case AssetType::LuaScript:
                case AssetType::CppCode:
                    return ICON_MD_TERMINAL;
                default:
                    return ICON_MD_INSERT_DRIVE_FILE;
            }
        }

        const char *GetAssetTypeName(AssetType type)
        {
            switch (type)
            {
                case AssetType::Mesh:
                    return "Mesh";
                case AssetType::Texture:
                    return "Texture";
                case AssetType::Shader:
                    return "Shader";
                case AssetType::LuaScript:
                    return "Lua Script";
                case AssetType::CppCode:
                    return "C++ Code";
                case AssetType::VFX:
                    return "VFX";
                case AssetType::Audio:
                    return "Audio";
                case AssetType::SkeletalAnimation:
                    return "Skeletal Animation";
                case AssetType::Scene:
                    return "Scene";
                case AssetType::TemplateAsset:
                    return "Template";
                case AssetType::ConfigFile:
                    return "Config";
                case AssetType::Font:
                    return "Font";
                case AssetType::Material:
                    return "Material";
                case AssetType::Library:
                    return "Library";
                case AssetType::AnimationStateMachine:
                    return "Animation State Machine";
                default:
                    return "Unknown";
            }
        }

        const char *GetFormatName(VkFormat format)
        {
            switch (format)
            {
                case VK_FORMAT_R8G8B8A8_UNORM:
                    return "RGBA8_UNORM";
                case VK_FORMAT_R8G8B8_UNORM:
                    return "RGB8_UNORM";
                case VK_FORMAT_R8G8_UNORM:
                    return "RG8_UNORM";
                case VK_FORMAT_R8_UNORM:
                    return "R8_UNORM";
                case VK_FORMAT_R16G16B16A16_SFLOAT:
                    return "RGBA16_SFLOAT";
                case VK_FORMAT_R32G32B32A32_SFLOAT:
                    return "RGBA32_SFLOAT";
                case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
                    return "BC1_RGB";
                case VK_FORMAT_BC3_UNORM_BLOCK:
                    return "BC3_UNORM";
                default:
                    return "Unknown Format";
            }
        }

        const char *GetFilterName(VkFilter filter)
        {
            switch (filter)
            {
                case VK_FILTER_NEAREST:
                    return "Nearest";
                case VK_FILTER_LINEAR:
                    return "Linear";
                case VK_FILTER_CUBIC_IMG:
                    return "Cubic";
                default:
                    return "Unknown";
            }
        }

        const char *GetAddressModeName(VkSamplerAddressMode mode)
        {
            switch (mode)
            {
                case VK_SAMPLER_ADDRESS_MODE_REPEAT:
                    return "Repeat";
                case VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
                    return "Mirrored Repeat";
                case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
                    return "Clamp to Edge";
                case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
                    return "Clamp to Border";
                case VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
                    return "Mirror Clamp to Edge";
                default:
                    return "Unknown";
            }
        }
    } // namespace

    void AssetBrowser::Draw()
    {
        if (!ProjectManager::Get()->IsAProjectLoaded())
            return;

        EnsureIconsLoaded();

        if (!ImGui::Begin(kWindowTitle, nullptr, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        if (m_currentPath.empty())
            NavigateTo(ProjectManager::Get()->GetProjectAssetPath());

        DrawMenuBar();
        DrawToolbar();
        ImGui::Separator();

        // A pending "new scene" tile only makes sense in the folder it was started in.
        if (m_inlineEditMode == InlineEditMode::NewScene && m_inlineEditPath != m_currentPath)
            EndInlineEdit();

        DrawPathNavigation();
        ImGui::Separator();

        DrawFilterControls();
        ImGui::Separator();

        if (m_inlineEditMode != InlineEditMode::None && ImGui::IsKeyPressed(ImGuiKey_Escape))
            EndInlineEdit();

        RefreshDirectoryCache();
        DrawContent();

        ImGui::End();

        DrawDeleteConfirmPopup();

        if (showCS)
            ShowCreateShaderWizard(m_currentPath);
        if (showCSI)
            ShowCreateShaderIncludeWizard(m_currentPath);
    }

    void AssetBrowser::EnsureIconsLoaded()
    {
        if (!m_icons.save)
            m_icons.save = GetSaveLogo();
        if (!m_icons.newFile)
            m_icons.newFile = GetNewLogo();
        if (!m_icons.hpp)
            m_icons.hpp = GetHppLogo();
        if (!m_icons.h)
            m_icons.h = GetHlogo();
        if (!m_icons.folder)
            m_icons.folder = GetFolderLogo();
        if (!m_icons.file)
            m_icons.file = GetFileLogo();
        if (!m_icons.c)
            m_icons.c = GetCLogo();
        if (!m_icons.cpp)
            m_icons.cpp = GetCppLogo();
        if (!m_icons.scene)
            m_icons.scene = GetSceneLogo();
    }

    void AssetBrowser::DrawContent()
    {
        const bool showDetails = m_viewMode == ViewMode::Details && m_selectedAsset.has_value();

        if (showDetails)
        {
            ImGui::Columns(2, "AssetSplitter", true);
            // Only set the width when the panel opens; doing it every frame makes the splitter undraggable.
            if (!m_detailsOpen)
                ImGui::SetColumnWidth(0, ImGui::GetContentRegionAvail().x * 0.6f);

            DrawAssetGrid();
            ImGui::NextColumn();
            DrawDetailsPanel();

            ImGui::Columns(1);
        } else
        {
            DrawAssetGrid();
        }

        m_detailsOpen = showDetails;
    }

    void AssetBrowser::DrawMenuBar()
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu(ICON_MD_INSERT_DRIVE_FILE " File"))
            {
                if (ImGui::BeginMenu(ICON_MD_ADD " New Asset"))
                {
                    DrawCreateAssetItems();
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(ICON_MD_REFRESH " Refresh"))
                    RefreshAssets();
                if (ImGui::MenuItem(ICON_MD_SAVE " Save All"))
                    AssetController::Get()->SaveAll();
                if (ImGui::MenuItem(ICON_MD_PUBLIC " Scene"))
                    BeginNewScene();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                for (const ViewModeInfo &info: kViewModes)
                    if (ImGui::MenuItem(info.menuLabel, nullptr, m_viewMode == info.mode))
                        m_viewMode = info.mode;

                ImGui::Separator();
                ImGui::MenuItem(ICON_MD_IMAGE " Show Thumbnails", nullptr, &m_showThumbnails);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(ICON_MD_HELP " Help"))
            {
                ImGui::MenuItem(ICON_MD_INFO " About", nullptr, false);
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        // Opened by the "+" button in the toolbar.
        if (ImGui::BeginPopup("CreateAssetPopup"))
        {
            DrawCreateAssetItems();
            ImGui::EndPopup();
        }
    }

    void AssetBrowser::DrawCreateAssetItems()
    {
        if (ImGui::MenuItem(ICON_MD_CROP_ORIGINAL " Image2D"))
            CreateImageAsset<Image2d>();
        if (ImGui::MenuItem(ICON_MD_CROP " Image3D"))
            CreateImageAsset<Image3d>();
        if (ImGui::MenuItem(ICON_MD_VIEW_COLUMN " Image2D Array"))
            CreateImageAsset<Image2dArray>();
        if (ImGui::MenuItem(ICON_MD_3D_ROTATION " Cubemap"))
            CreateImageAsset<Cubemap>();
    }

    void AssetBrowser::DrawToolbar()
    {
        ImGui::BeginGroup();

        bool first = true;
        for (const ViewModeInfo &info: kViewModes)
        {
            if (!first)
                ImGui::SameLine();
            first = false;

            const bool active = m_viewMode == info.mode;
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImGui::GetStyleColorVec4(active ? ImGuiCol_ButtonActive : ImGuiCol_Button));
            if (ImGui::Button(info.icon))
                m_viewMode = info.mode;
            ImGui::PopStyleColor();
        }

        ToolbarGap();

        if (ImGui::Button(ICON_MD_ADD))
            ImGui::OpenPopup("CreateAssetPopup");
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_REFRESH))
            RefreshAssets();

        ToolbarGap();

        ImGui::SetNextItemWidth(120);
        ImGui::SliderFloat(ICON_MD_ZOOM_IN, &m_thumbnailSize, 32.0f, 200.0f, "%.0f");

        ToolbarGap();

        ImGui::Checkbox(ICON_MD_IMAGE " Thumbs", &m_showThumbnails);

        ImGui::EndGroup();
    }

    void AssetBrowser::DrawPathNavigation()
    {
        ImGui::BeginGroup();

        // TODO: real back/forward history. For now Back behaves like Up and Forward does nothing.
        if (ImGui::ArrowButton("##back", ImGuiDir_Left))
            NavigateUp();
        ImGui::SameLine();
        ImGui::ArrowButton("##forward", ImGuiDir_Right);
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_ARROW_UPWARD))
            NavigateUp();
        ImGui::SameLine();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 150);
        // Note: the 3rd argument of InputText is the buffer *size*, flags come 4th.
        ImGui::InputText("##path", m_currentPathText.data(), m_currentPathText.size() + 1,
                         ImGuiInputTextFlags_ReadOnly);

        ImGui::EndGroup();
    }

    void AssetBrowser::DrawFilterControls()
    {
        ImGui::BeginGroup();

        ImGui::SetNextItemWidth(250);
        if (ImGui::InputTextWithHint("##search", ICON_MD_SEARCH " Search assets...", m_searchBuffer,
                                     sizeof(m_searchBuffer)))
            m_searchFilter = ToLower(m_searchBuffer);

        ImGui::SameLine();

        int currentType = static_cast<int>(m_typeFilter) + 1; // +1 because -1 = All
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("##typefilter", &currentType, kTypeFilterNames, IM_ARRAYSIZE(kTypeFilterNames)))
            m_typeFilter = static_cast<AssetType>(currentType - 1);

        ImGui::EndGroup();
    }


    void AssetBrowser::NavigateTo(const std::filesystem::path &path)
    {
        if (path == m_currentPath)
            return;
        m_currentPath     = path;
        m_currentPathText = path.string();
    }

    void AssetBrowser::NavigateUp()
    {
        if (m_currentPath.has_parent_path())
            NavigateTo(m_currentPath.parent_path());
    }

    void AssetBrowser::RefreshAssets()
    {
        m_previewCache.clear();
        m_dirDirty = true;
    }

    // Listing a directory is a syscall storm; doing it every frame (as before, twice) is what made
    // the browser slow. We now rebuild only when the path changes, something marks the cache dirty,
    // or the folder's mtime changes (polled a couple of times per second).
    void AssetBrowser::RefreshDirectoryCache()
    {
        namespace fs = std::filesystem;

        const double now = ImGui::GetTime();
        std::error_code ec;

        if (!m_dirDirty && m_cachedPath == m_currentPath)
        {
            if (now < m_nextDirPoll)
                return;
            m_nextDirPoll = now + kDirPollIntervalSec;

            const auto stamp = fs::last_write_time(m_currentPath, ec);
            if (!ec && stamp == m_cachedStamp)
                return;
        }

        m_dirDirty    = false;
        m_cachedPath  = m_currentPath;
        m_nextDirPoll = now + kDirPollIntervalSec;
        m_cachedStamp = fs::last_write_time(m_currentPath, ec);

        m_folders.clear();
        m_sceneFiles.clear();

        for (fs::directory_iterator it(m_currentPath, ec), end; !ec && it != end; it.increment(ec))
        {
            std::error_code entryEc;
            const fs::path &path = it->path();

            if (it->is_directory(entryEc))
                m_folders.push_back(DirEntry{path, path.filename().string()});
            else if (it->is_regular_file(entryEc) && IsSceneFile(path))
                m_sceneFiles.push_back(DirEntry{path, SceneDisplayName(path)});
        }

        const auto byName = [](const DirEntry &a, const DirEntry &b) { return LessNoCase(a.name, b.name); };
        std::sort(m_folders.begin(), m_folders.end(), byName);
        std::sort(m_sceneFiles.begin(), m_sceneFiles.end(), byName);
    }

    std::filesystem::path AssetBrowser::CreateUniqueFolder(const std::filesystem::path &parent,
                                                           const std::string &baseName)
    {
        std::filesystem::path candidate = parent / baseName;
        int suffix                      = 1;
        while (File::Exists(candidate.string()))
            candidate = parent / (baseName + " (" + std::to_string(suffix++) + ")");
        return candidate;
    }

    void AssetBrowser::OpenSceneFile(const std::filesystem::path &file)
    {
        if (!OpenScene(file))
            Log::Error("Failed to open scene: {}", file.string());
    }

    // ======================================================================
    //  Content area
    // ======================================================================

    void AssetBrowser::DrawAssetGrid()
    {
        const auto controller = AssetController::Get();
        if (controller == nullptr)
            return;

        ImGui::BeginChild("AssetGrid", Vec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (m_viewMode == ViewMode::List)
            DrawListView(controller->assets_);
        else
            DrawGridView(controller->assets_);

        // Right-click on empty grid space -> "New" menu
        if (ImGui::BeginPopupContextWindow("AssetBrowserContext",
                                           ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawContextMenu();
            ImGui::EndPopup();
        }

        ImGui::EndChild();
    }

    void AssetBrowser::DrawContextMenu()
    {
        if (!ImGui::BeginMenu("New"))
            return;

        if (ImGui::MenuItem("Folder"))
        {
            const auto path = CreateUniqueFolder(m_currentPath, "New Folder");
            if (File::CreateDirectory(path.string()))
            {
                m_dirDirty = true;
                BeginRenameFolder(path);
            }
        }
        if (ImGui::MenuItem("Prefab"))
        {
            // TODO: ShowPrefabWizard();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Shader"))
            showCS = true;
        if (ImGui::MenuItem("Shader Include"))
            showCSI = true;

        if (ImGui::BeginMenu("Script"))
        {
            // TODO: hook these up to script-creation wizards
            ImGui::MenuItem("C++ Header");
            ImGui::MenuItem("C++ Source");
            ImGui::MenuItem("Lua Source");
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Scene"))
            BeginNewScene();

        ImGui::EndMenu();
    }

    bool AssetBrowser::ShouldShowAsset(const std::shared_ptr<AssetBase> &asset) const
    {
        if (m_typeFilter != kAllTypes && asset->type != m_typeFilter)
            return false;
        return ContainsNoCase(asset->name, m_searchFilter);
    }

    // ======================================================================
    //  Grid view
    // ======================================================================

    void AssetBrowser::DrawGridView(const AssetList &assets)
    {
        const float cellSize = m_thumbnailSize + 40.0f;
        const int columns    = std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / cellSize));

        if (!ImGui::BeginTable("AssetGrid", columns, ImGuiTableFlags_None))
            return;

        for (const DirEntry &folder: m_folders)
        {
            ImGui::TableNextColumn();
            DrawFolderTile(folder);
        }

        if (IsCreatingSceneHere())
        {
            ImGui::TableNextColumn();
            DrawNewSceneTile();
        }

        // Scene files live on disk, not in AssetController, so they need their own tiles.
        for (const DirEntry &scene: m_sceneFiles)
        {
            ImGui::TableNextColumn();
            DrawSceneFileTile(scene);
        }

        int index = 0;
        for (const auto &asset: assets)
        {
            if (!asset || !ShouldShowAsset(asset))
                continue;
            ImGui::TableNextColumn();
            DrawAssetTile(asset, index++);
        }

        ImGui::EndTable();
    }

    // Shared layout for folder / scene-file tiles: icon on top, label (or inline-edit field) below.
    // Leaves the whole tile as the "last item", so callers can attach a context menu right after.
    bool AssetBrowser::DrawIconTile(const std::shared_ptr<Image2d> &icon, const std::string &name, bool editing,
                                    bool highlighted)
    {
        const Vec2 thumbnailSize(m_thumbnailSize, m_thumbnailSize);
        const Vec2 tileSize(thumbnailSize.x, thumbnailSize.y + ImGui::GetTextLineHeight() + kTileLabelPadding);
        const Vec2 origin = ImGui::GetCursorPos();

        // A real interactive item covering the whole tile (Image/Text aren't clickable on their own).
        ImGui::InvisibleButton("##tile", tileSize);
        const bool doubleClicked = !editing && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0);

        ImGui::SetCursorPos(origin);
        ImGui::BeginGroup();

        DrawIcon(icon, thumbnailSize);

        if (editing)
        {
            DrawInlineEditField(thumbnailSize.x);
        } else
        {
            if (highlighted)
                ImGui::PushStyleColor(ImGuiCol_Text, kCurrentSceneColor);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize.x);
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::PopTextWrapPos();
            if (highlighted)
                ImGui::PopStyleColor();
        }

        ImGui::EndGroup();
        return doubleClicked;
    }

    void AssetBrowser::DrawFolderTile(const DirEntry &folder)
    {
        const ScopedId id("folder", folder.name);
        const bool editing = IsEditing(InlineEditMode::RenameFolder, folder.path);

        if (DrawIconTile(m_icons.folder, folder.name, editing))
            NavigateTo(folder.path);

        if (!editing)
            DrawFolderContextMenu(folder);
    }

    void AssetBrowser::DrawSceneFileTile(const DirEntry &scene)
    {
        const ScopedId id("scene", scene.name);
        const bool editing = IsEditing(InlineEditMode::RenameScene, scene.path);

        if (DrawIconTile(m_icons.scene, scene.name, editing, IsFileCurrentCurrentScene(scene.path)))
            OpenSceneFile(scene.path);

        if (!editing)
            DrawSceneContextMenu(scene);
    }

    void AssetBrowser::DrawNewSceneTile()
    {
        ImGui::PushID("newscene_tile");
        DrawIconTile(m_icons.scene, {}, /*editing=*/true);
        ImGui::PopID();
    }

    void AssetBrowser::DrawAssetTile(const std::shared_ptr<AssetBase> &asset, int index)
    {
        const Vec2 thumbnailSize(m_thumbnailSize, m_thumbnailSize);
        const bool isSelected = m_selectedAsset && *m_selectedAsset == asset->uuid;

        ImGui::PushID(index);
        ImGui::BeginGroup();

        DrawAssetThumbnail(asset, thumbnailSize);
        if (ImGui::IsItemClicked()) // the thumbnail is the last item here, whichever branch drew it
            m_selectedAsset = asset->uuid;

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize.x);
        ImGui::TextWrapped("%s", asset->name.c_str()); // TODO: click = rename
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();

        if (isSelected)
            DrawSelectionOutline();

        DrawAssetContextMenu(asset);
        ImGui::PopID();
    }

    // Draws exactly one item: the real preview, the scene logo, or a placeholder button.
    void AssetBrowser::DrawAssetThumbnail(const std::shared_ptr<AssetBase> &asset, Vec2 size)
    {
        if (m_showThumbnails && TryDrawThumbnail(asset, size))
            return;

        if (asset->type == AssetType::Scene && m_icons.scene)
        {
            ImGui::Image(m_icons.scene->GetTexID(), size);
            return;
        }

        DrawPlaceholder(asset, "##thumb", size, Vec4(0.2f, 0.2f, 0.2f, m_showThumbnails ? 1.0f : 0.0f));
    }

    // Flat button with the asset-type glyph centred on it. The glyph is painted through the draw list,
    // so the button stays the "last item" (the old SetCursorPos dance also placed it below the button).
    void AssetBrowser::DrawPlaceholder(const std::shared_ptr<AssetBase> &asset, const char *id, Vec2 size,
                                       const Vec4 &color)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, color);
        ImGui::Button(id, size);
        ImGui::PopStyleColor();

        const char *icon = GetAssetIcon(asset->type);
        const Vec2 glyph = ImGui::CalcTextSize(icon);
        const Vec2 min   = ImGui::GetItemRectMin();
        const Vec2 max   = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddText(
                Vec2(min.x + (max.x - min.x - glyph.x) * 0.5f, min.y + (max.y - min.y - glyph.y) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), icon);
    }

    // Draws the texture preview if the asset is an image with a ready preview. True if something was drawn.
    bool AssetBrowser::TryDrawThumbnail(const std::shared_ptr<AssetBase> &asset, Vec2 box)
    {
        bool drawn = false;
        VisitImageAsset(asset,
                        [&](const auto &image)
                        {
                            if (!image->texture)
                                return;
                            const TexturePreview &preview = GetOrCreatePreview(image->uuid, image->texture);
                            if (!preview.isValid || !preview.textureID)
                                return;

                            ImGui::Image(preview.textureID, FitInside(Vec2(preview.size), box, true));
                            drawn = true;
                        });
        return drawn;
    }

    void AssetBrowser::DrawFolderContextMenu(const DirEntry &folder)
    {
        if (!ImGui::BeginPopupContextItem("FolderContext"))
            return;

        if (ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open"))
            NavigateTo(folder.path);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_DRIVE_FILE_RENAME_OUTLINE " Rename"))
            BeginRenameFolder(folder.path);
        if (ImGui::MenuItem(ICON_MD_DELETE " Delete"))
            RequestDelete(folder.path, /*isScene=*/false);

        ImGui::EndPopup();
    }

    void AssetBrowser::DrawSceneContextMenu(const DirEntry &scene)
    {
        if (!ImGui::BeginPopupContextItem("SceneFileContext"))
            return;

        if (ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open"))
            OpenSceneFile(scene.path);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_DRIVE_FILE_RENAME_OUTLINE " Rename"))
            BeginRenameScene(scene.path);
        if (ImGui::MenuItem(ICON_MD_DELETE " Delete"))
            RequestDelete(scene.path, /*isScene=*/true);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Copy Path"))
            ImGui::SetClipboardText(scene.path.string().c_str());

        ImGui::EndPopup();
    }

    void AssetBrowser::DrawAssetContextMenu(const std::shared_ptr<AssetBase> &asset)
    {
        if (!ImGui::BeginPopupContextItem("AssetContext"))
            return;

        if (ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open"))
            m_selectedAsset = asset->uuid;
        ImGui::MenuItem(ICON_MD_DELETE " Delete"); // TODO: asset deletion
        if (ImGui::MenuItem(ICON_MD_SAVE " Save"))
            asset->Save();
        if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Copy UUID"))
            ImGui::SetClipboardText(asset->uuid.ToString().c_str());

        ImGui::EndPopup();
    }

    // ======================================================================
    //  List view
    // ======================================================================

    void AssetBrowser::DrawListView(const AssetList &assets)
    {
        constexpr ImGuiTableFlags flags =
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
        if (!ImGui::BeginTable("AssetList", 4, flags))
            return;

        ImGui::TableSetupColumn(ICON_MD_IMAGE " Preview", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn(ICON_MD_TITLE " Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(ICON_MD_CATEGORY " Type", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn(ICON_MD_SETTINGS " Actions", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableHeadersRow();

        for (const DirEntry &folder: m_folders)
            DrawFolderRow(folder);

        if (IsCreatingSceneHere())
            DrawNewSceneRow();

        for (const DirEntry &scene: m_sceneFiles)
            DrawSceneRow(scene);

        int index = 0;
        for (const auto &asset: assets)
        {
            if (!asset || !ShouldShowAsset(asset))
                continue;
            DrawAssetRow(asset, index++);
        }

        ImGui::EndTable();
    }

    void AssetBrowser::DrawFolderRow(const DirEntry &folder)
    {
        const ScopedId id("folder", folder.name);
        const bool editing = IsEditing(InlineEditMode::RenameFolder, folder.path);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(ICON_MD_FOLDER);

        ImGui::TableSetColumnIndex(1);
        if (editing)
        {
            DrawInlineEditField(-1); // -1 = fill remaining column width
        } else if (ImGui::Selectable(folder.name.c_str(), false,
                                     ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick) &&
                   ImGui::IsMouseDoubleClicked(0))
        {
            NavigateTo(folder.path);
        }

        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("Folder");

        ImGui::TableSetColumnIndex(3);
        if (!editing)
        {
            if (ImGui::SmallButton(ICON_MD_DRIVE_FILE_RENAME_OUTLINE "##rename"))
                BeginRenameFolder(folder.path);
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_MD_DELETE "##delete"))
                RequestDelete(folder.path, /*isScene=*/false);
        }
    }

    void AssetBrowser::DrawNewSceneRow()
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawIcon(m_icons.scene, Vec2(24, 24));

        ImGui::TableSetColumnIndex(1);
        DrawInlineEditField(-1);

        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("Scene");
    }

    // Scene files used to be missing from the list view entirely.
    void AssetBrowser::DrawSceneRow(const DirEntry &scene)
    {
        const ScopedId id("scene", scene.name);
        const bool editing = IsEditing(InlineEditMode::RenameScene, scene.path);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawIcon(m_icons.scene, Vec2(24, 24));

        ImGui::TableSetColumnIndex(1);
        if (editing)
        {
            DrawInlineEditField(-1);
        } else
        {
            const bool current = IsFileCurrentCurrentScene(scene.path);
            if (current)
                ImGui::PushStyleColor(ImGuiCol_Text, kCurrentSceneColor);
            const bool clicked =
                    ImGui::Selectable(scene.name.c_str(), false,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
            if (current)
                ImGui::PopStyleColor();

            if (clicked && ImGui::IsMouseDoubleClicked(0))
                OpenSceneFile(scene.path);
        }

        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted("Scene");

        ImGui::TableSetColumnIndex(3);
        if (!editing)
        {
            if (ImGui::SmallButton(ICON_MD_DRIVE_FILE_RENAME_OUTLINE "##rename"))
                BeginRenameScene(scene.path);
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_MD_DELETE "##delete"))
                RequestDelete(scene.path, /*isScene=*/true);
        }
    }

    void AssetBrowser::DrawAssetRow(const std::shared_ptr<AssetBase> &asset, int index)
    {
        ImGui::PushID(index);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        DrawSmallPreview(asset);

        ImGui::TableSetColumnIndex(1);
        const bool isSelected = m_selectedAsset && *m_selectedAsset == asset->uuid;
        if (ImGui::Selectable(asset->name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
            m_selectedAsset = asset->uuid;

        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(GetAssetTypeName(asset->type));

        ImGui::TableSetColumnIndex(3);
        if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW "##open"))
            m_selectedAsset = asset->uuid;
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_SAVE "##save"))
            asset->Save();

        ImGui::PopID();
    }

    void AssetBrowser::DrawSmallPreview(const std::shared_ptr<AssetBase> &asset)
    {
        const Vec2 size(kListPreviewSize, kListPreviewSize);
        if (!TryDrawThumbnail(asset, size))
            DrawPlaceholder(asset, "##preview", size, Vec4(0.2f, 0.2f, 0.2f, 0.5f));
    }

    // ======================================================================
    //  Details panel
    // ======================================================================

    void AssetBrowser::DrawDetailsPanel()
    {
        ImGui::BeginChild("DetailsPanel", Vec2(0, 0), true);

        const auto asset = AssetController::Get()->FindByUUID(*m_selectedAsset);
        if (!asset)
            ImGui::TextColored(kErrorColor, "%s", ICON_MD_ERROR " Asset not found");
        else if (!VisitImageAsset(asset, [this](const auto &image) { DrawImageDetails(image); }))
            DrawGenericAssetDetails(asset);

        ImGui::EndChild();
    }

    template<typename TImage>
    void AssetBrowser::DrawImageDetails(const std::shared_ptr<ImageAsset<TImage>> &asset)
    {
        ImGui::BeginGroup();

        Heading(ICON_MD_IMAGE " Image Asset Details");
        ImGui::Separator();

        const auto &tex = asset->texture;

        if (tex)
        {
            const TexturePreview &preview = GetOrCreatePreview(asset->uuid, tex);
            if (preview.isValid && preview.textureID)
            {
                ImGui::TextUnformatted(ICON_MD_VISIBILITY " Preview:");

                const Vec2 box(ImGui::GetContentRegionAvail().x - 20, kDetailsPreviewMaxHeight);
                ImGui::Image(preview.textureID, FitInside(Vec2(preview.size), box, /*allowUpscale=*/false));
                ImGui::Separator();
            }
        }

        Heading(ICON_MD_INFO " Properties");
        if (BeginPropertyTable("ImageProperties"))
        {
            PropertyRow(ICON_MD_TITLE " Name", asset->name);
            PropertyRow(ICON_MD_CATEGORY " Type", GetAssetTypeName(asset->type));

            if (tex)
            {
                const auto extent = tex->GetExtent();

                PropertyRow(ICON_MD_CROP_ORIGINAL " Dimensions",
                            std::to_string(extent.x) + "x" + std::to_string(extent.y) + "x" + std::to_string(extent.z));
                PropertyRow(ICON_MD_FORMAT_COLOR_FILL " Format", GetFormatName(tex->GetFormat()));
                PropertyRow(ICON_MD_LAYERS " Mip Levels", std::to_string(tex->GetMipLevels()));
                PropertyRow(ICON_MD_VIEW_COLUMN " Array Layers", std::to_string(tex->GetArrayLevels()));
                PropertyRow(ICON_MD_FILTER " Filter", GetFilterName(tex->GetFilter()));
                PropertyRow(ICON_MD_BORDER_ALL " Address Mode", GetAddressModeName(tex->GetAddressMode()));
                PropertyRow(ICON_MD_LABEL " Samples", std::to_string(tex->GetSamples()));
                PropertyRow(ICON_MD_SETTINGS " Usage", ToHex(static_cast<std::uint32_t>(tex->GetUsage())));
            }

            ImGui::EndTable();
        }

        ImGui::Separator();
        DrawActionButtons(asset);

        ImGui::EndGroup();
    }

    void AssetBrowser::DrawGenericAssetDetails(const std::shared_ptr<AssetBase> &asset)
    {
        ImGui::BeginGroup();

        Heading(ICON_MD_INFO " Asset Details");
        ImGui::Separator();

        if (BeginPropertyTable("AssetProperties"))
        {
            PropertyRow(ICON_MD_TITLE " Name", asset->name);
            PropertyRow(ICON_MD_CATEGORY " Type", GetAssetTypeName(asset->type));
            ImGui::EndTable();
        }

        ImGui::Separator();
        DrawActionButtons(asset);

        ImGui::EndGroup();
    }

    template<typename T>
    void AssetBrowser::DrawActionButtons(const std::shared_ptr<T> &asset)
    {
        Heading(ICON_MD_SETTINGS " Actions");

        if (ImGui::Button(ICON_MD_SAVE " Save", Vec2(100, 0)))
            asset->Save();
        ImGui::SameLine();
        ImGui::Button(ICON_MD_DELETE " Delete", Vec2(100, 0)); // TODO: asset deletion
    }

    // ======================================================================
    //  Asset creation + previews
    // ======================================================================

    // TODO: AssetWizard class
    template<typename TImage>
    void AssetBrowser::CreateImageAsset()
    {
        static_assert(std::is_base_of_v<Image, TImage>, "TImage must derive from Image");

        auto asset = AssetController::Get()->RegisterAsset<ImageAsset<TImage>>(
                "New" + std::string(GetAssetTypeName(AssetType::Texture)));

        // Only 3D images take a 3D extent. Cubemap uses 2D dimensions (square face size).
        if constexpr (std::is_same_v<TImage, Image3d>)
            asset->Create(UVec3(kDefaultImageSize, kDefaultImageSize, kDefaultImageSize), kDefaultImageFormat);
        else
            asset->Create(UVec2(kDefaultImageSize, kDefaultImageSize), kDefaultImageFormat);

        asset->Save();
        m_selectedAsset = asset->uuid;
    }

    // Entries (including invalid ones) are cached until RefreshAssets() clears the cache, so a lookup is
    // one hash find per call instead of a map write every frame.
    // NOTE: once real GPU upload exists and previews can become valid later, re-try invalid entries here.
    template<typename TImage>
    const AssetBrowser::TexturePreview &AssetBrowser::GetOrCreatePreview(const UUID &guid,
                                                                         const std::shared_ptr<TImage> &texture)
    {
        if (const auto it = m_previewCache.find(guid); it != m_previewCache.end())
            return it->second;

        TexturePreview preview;
        preview.isValid = false;
        preview.size    = texture->GetExtent();

        // TODO: create an ImGui texture ID from the Vulkan image (integrate with the rendering system),
        // then set preview.textureID and preview.isValid.

        return m_previewCache.emplace(guid, preview).first->second;
    }

    // ======================================================================
    //  Inline edit (rename folder / rename scene / new scene)
    // ======================================================================

    void AssetBrowser::BeginInlineEdit(InlineEditMode mode, const std::filesystem::path &path, const std::string &text)
    {
        m_inlineEditMode = mode;
        m_inlineEditPath = path;
        std::snprintf(m_inlineEditBuffer, sizeof(m_inlineEditBuffer), "%s", text.c_str());
        m_inlineEditFocusPending = true;
    }

    void AssetBrowser::EndInlineEdit()
    {
        m_inlineEditMode = InlineEditMode::None;
        m_inlineEditPath.clear();
    }

    bool AssetBrowser::IsEditing(InlineEditMode mode, const std::filesystem::path &path) const
    {
        return m_inlineEditMode == mode && m_inlineEditPath == path;
    }

    bool AssetBrowser::IsCreatingSceneHere() const
    {
        return m_inlineEditMode == InlineEditMode::NewScene && m_inlineEditPath == m_currentPath;
    }

    void AssetBrowser::DrawInlineEditField(float width)
    {
        ImGui::SetNextItemWidth(width);

        if (m_inlineEditFocusPending)
        {
            ImGui::SetKeyboardFocusHere();
            m_inlineEditFocusPending = false;
        }

        const bool confirmed =
                ImGui::InputText("##inlineedit", m_inlineEditBuffer, sizeof(m_inlineEditBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        // Enter confirms; so does clicking away. IsItemDeactivated() is only true for a field that was
        // active, so no "armed" flag is needed. (The old local `m_inlineEditArmed` shadowed nothing and was
        // reset every frame, so click-away never committed.)
        if (confirmed || ImGui::IsItemDeactivated())
            CommitInlineEdit();
    }

    void AssetBrowser::CommitInlineEdit()
    {
        switch (m_inlineEditMode)
        {
            case InlineEditMode::RenameFolder:
                CommitFolderRename();
                break;
            case InlineEditMode::NewScene:
                CommitNewScene();
                break;
            case InlineEditMode::RenameScene:
                CommitSceneRename();
                break;
            default:
                break;
        }
    }

    void AssetBrowser::BeginNewScene()
    {
        const auto dir = m_currentPath.empty() ? ProjectManager::Get()->GetProjectAssetPath() : m_currentPath;
        BeginInlineEdit(InlineEditMode::NewScene, dir, "New Scene");
        ImGui::SetWindowFocus(kWindowTitle); // bring the tab forward if it's docked behind another
    }

    void AssetBrowser::CommitNewScene()
    {
        const std::string name = CleanFileName(m_inlineEditBuffer);

        if (!name.empty()) // empty name = treat as cancel
        {
            if (CreateDefaultScene(m_inlineEditPath, name).empty())
                Log::Error("Failed to create scene in {}", m_inlineEditPath.string());
            else
                RefreshAssets();
        }

        EndInlineEdit();
    }

    void AssetBrowser::BeginRenameFolder(const std::filesystem::path &path)
    {
        BeginInlineEdit(InlineEditMode::RenameFolder, path, path.filename().string());
    }

    void AssetBrowser::CommitFolderRename()
    {
        // CleanFileName also strips path separators, so "a/b" can no longer move the folder somewhere else.
        const std::string newName = CleanFileName(m_inlineEditBuffer);

        if (!newName.empty() && newName != m_inlineEditPath.filename().string())
        {
            const std::filesystem::path newPath = m_inlineEditPath.parent_path() / newName;
            if (!File::Exists(newPath.string()) && File::Rename(m_inlineEditPath.string(), newPath.string()))
            {
                if (m_currentPath == m_inlineEditPath)
                    NavigateTo(newPath);
                m_dirDirty = true;
            }
        }

        EndInlineEdit();
    }

    void AssetBrowser::BeginRenameScene(const std::filesystem::path &file)
    {
        BeginInlineEdit(InlineEditMode::RenameScene, file, SceneDisplayName(file));
    }

    void AssetBrowser::CommitSceneRename()
    {
        namespace fs           = std::filesystem;
        const std::string name = CleanFileName(m_inlineEditBuffer);

        if (!name.empty())
        {
            const fs::path dst = m_inlineEditPath.parent_path() / (name + std::string(SceneFileExtension));
            std::error_code ec;
            if (dst != m_inlineEditPath && !fs::exists(dst, ec))
            {
                fs::rename(m_inlineEditPath, dst, ec);
                if (ec)
                {
                    Log::Error("Failed to rename scene: {}", ec.message());
                } else
                {
                    NotifyMovedScene(m_inlineEditPath, dst);
                    m_dirDirty = true;
                }
            }
        }

        EndInlineEdit();
    }

    // ======================================================================
    //  Delete confirmation
    // ======================================================================

    void AssetBrowser::RequestDelete(const std::filesystem::path &path, bool isScene)
    {
        m_deleteTargetPath       = path;
        m_deleteTargetIsScene    = isScene; // previously never reset when deleting a folder after a scene
        m_showDeleteConfirmPopup = true;
    }

    void AssetBrowser::DrawDeleteConfirmPopup()
    {
        if (m_showDeleteConfirmPopup)
        {
            ImGui::OpenPopup("Confirm Delete");
            m_showDeleteConfirmPopup = false;
        }

        ImGui::SetNextWindowSize(Vec2(360, 0), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        ImGui::TextColored(kWarningColor, "%s",
                           m_deleteTargetIsScene ? ICON_MD_WARNING " Delete this scene file?"
                                                 : ICON_MD_WARNING " Delete this folder and all its contents?");
        ImGui::TextWrapped("%s", m_deleteTargetPath.string().c_str());
        ImGui::Separator();

        if (ImGui::Button(ICON_MD_DELETE " Delete", Vec2(120, 0)))
        {
            ExecuteDelete();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_CLOSE " Cancel", Vec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    void AssetBrowser::ExecuteDelete()
    {
        if (m_deleteTargetIsScene)
        {
            std::error_code ec;
            std::filesystem::remove(m_deleteTargetPath, ec);
            if (ec)
                Log::Error("Failed to delete {}: {}", m_deleteTargetPath.string(), ec.message());
            else
                NotifyDeletedScene(m_deleteTargetPath);
        } else
        {
            // Component-wise check; the old string-prefix test also matched "/a/foobar" for "/a/foo".
            const bool viewingDeletedFolder = IsSameOrInside(m_currentPath, m_deleteTargetPath);

            File::DeleteDirectory(m_deleteTargetPath.string(), true);

            if (viewingDeletedFolder && m_deleteTargetPath.has_parent_path())
                NavigateTo(m_deleteTargetPath.parent_path());
        }

        m_dirDirty = true;
    }
} // namespace SF::Engine
