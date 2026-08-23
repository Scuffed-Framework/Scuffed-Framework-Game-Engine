#include "AssetsWindow.hpp"
#include <Project/Project.hpp>
#include <Gui/GuiMembers.hpp>
#include "../Wizzards/Shaders.hpp"
#include "Panels.hpp"

namespace SF::Engine
{
    void AssetBrowser::Draw()
    {
        if (ProjectManager::Get()->IsAProjectLoaded() == false)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Asset Controller not initialized");
            return;
        }
        if (!ImGui::Begin(ICON_MD_FOLDER " Asset Browser", nullptr, ImGuiWindowFlags_MenuBar))
        {
            ImGui::End();
            return;
        }

        DrawMenuBar();
        DrawToolbar();
        ImGui::Separator();

        if (m_currentPath.empty())
        {
            m_currentPath = ProjectManager::Get()->IsAProjectLoaded()
                                ? ProjectManager::Get()->GetProjectAssetPath()
                                : std::filesystem::current_path();
        }

        DrawPathNavigation();
        ImGui::Separator();

        DrawFilterControls();
        ImGui::Separator();

        if (m_inlineEditMode != InlineEditMode::None && ImGui::IsKeyPressed(ImGuiKey_Escape))
            CancelInlineFolderEdit();

        // Main content area with splitter for details panel
        if (m_viewMode == ViewMode::Details && m_selectedAsset)
        {
            ImGui::Columns(2, "AssetSplitter", true);
            ImGui::SetColumnWidth(0, ImGui::GetContentRegionAvail().x * 0.6f);

            DrawAssetGrid();

            ImGui::NextColumn();
            DrawDetailsPanel();

            ImGui::Columns(1);
        }
        else
        {
            DrawAssetGrid();
        }

        ImGui::End();
        DrawDeleteFolderConfirmPopup();

        if (showCS)
            ShowCreateShaderWizzard(m_currentPath);
        if (showCSI)
            ShowCreateShaderIncludeWizzard(m_currentPath);
    }

    template <typename Func>
    bool AssetBrowser::TryWithImageTexture(const std::shared_ptr<AssetBase> &asset, Func &&fn)
    {
        if (auto a = ::SF::RTTI::rtti_pointer_cast<ImageAsset<Image2d>>(asset))
            return a->texture ? (fn(a->texture, a->uuid), true) : false;
        if (auto a = ::SF::RTTI::rtti_pointer_cast<ImageAsset<Image3d>>(asset))
            return a->texture ? (fn(a->texture, a->uuid), true) : false;
        if (auto a = ::SF::RTTI::rtti_pointer_cast<ImageAsset<Image2dArray>>(asset))
            return a->texture ? (fn(a->texture, a->uuid), true) : false;
        if (auto a = ::SF::RTTI::rtti_pointer_cast<ImageAsset<Cubemap>>(asset))
            return a->texture ? (fn(a->texture, a->uuid), true) : false;
        return false;
    }

    void AssetBrowser::DrawMenuBar()
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu(ICON_MD_INSERT_DRIVE_FILE " File"))
            {
                if (ImGui::MenuItem(ICON_MD_ADD " New Asset"))
                {
                    ImGui::OpenPopup("CreateAssetPopup");
                }
                if (ImGui::MenuItem(ICON_MD_REFRESH " Refresh"))
                {
                    RefreshAssets();
                }
                if (ImGui::MenuItem(ICON_MD_SAVE " Save All"))
                {
                    AssetController::Get()->SaveAll();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem(ICON_MD_GRID_ON " Grid", nullptr, m_viewMode == ViewMode::Grid))
                    m_viewMode = ViewMode::Grid;
                if (ImGui::MenuItem(ICON_MD_FORMAT_LIST_BULLETED " List", nullptr, m_viewMode == ViewMode::List))
                    m_viewMode = ViewMode::List;
                if (ImGui::MenuItem(ICON_MD_DETAILS " Details", nullptr, m_viewMode == ViewMode::Details))
                    m_viewMode = ViewMode::Details;

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

        // Create asset popup
        if (ImGui::BeginPopup("CreateAssetPopup"))
        {
            if (ImGui::MenuItem(ICON_MD_CROP_ORIGINAL " Image2D"))
            {
                CreateImageAsset<Image2d>();
            }
            if (ImGui::MenuItem(ICON_MD_CROP " Image3D"))
            {
                CreateImageAsset<Image3d>();
            }
            if (ImGui::MenuItem(ICON_MD_VIEW_COLUMN " Image2D Array"))
            {
                CreateImageAsset<Image2dArray>();
            }
            if (ImGui::MenuItem(ICON_MD_3D_ROTATION " Cubemap"))
            {
                CreateImageAsset<Cubemap>();
            }
            ImGui::EndPopup();
        }
    }

    void AssetBrowser::DrawToolbar()
    {
        ImGui::BeginGroup();

        // View mode buttons with Material Design icons
        ImGui::PushStyleColor(ImGuiCol_Button, m_viewMode == ViewMode::Grid
                                                   ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]
                                                   : ImGui::GetStyle().Colors[ImGuiCol_Button]);
        if (ImGui::Button(ICON_MD_GRID_ON))
            m_viewMode = ViewMode::Grid;
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, m_viewMode == ViewMode::List
                                                   ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]
                                                   : ImGui::GetStyle().Colors[ImGuiCol_Button]);
        if (ImGui::Button(ICON_MD_FORMAT_LIST_BULLETED))
            m_viewMode = ViewMode::List;
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, m_viewMode == ViewMode::Details
                                                   ? ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]
                                                   : ImGui::GetStyle().Colors[ImGuiCol_Button]);
        if (ImGui::Button(ICON_MD_DETAILS))
            m_viewMode = ViewMode::Details;
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10, 0));
        ImGui::SameLine();

        if (ImGui::Button(ICON_MD_ADD))
        {
            ImGui::OpenPopup("CreateAssetPopup");
        }

        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_REFRESH))
        {
            RefreshAssets();
        }

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10, 0));
        ImGui::SameLine();

        ImGui::SetNextItemWidth(120);
        ImGui::SliderFloat(ICON_MD_ZOOM_IN, &m_thumbnailSize, 32.0f, 200.0f, "%.0f");

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(10, 0));
        ImGui::SameLine();

        ImGui::Checkbox(ICON_MD_IMAGE " Thumbs", &m_showThumbnails);

        ImGui::EndGroup();
    }

    void AssetBrowser::DrawPathNavigation()
    {
        ImGui::BeginGroup();

        // Back button
        if (ImGui::ArrowButton("##back", ImGuiDir_Left))
        {
            if (m_currentPath.has_parent_path())
                m_currentPath = m_currentPath.parent_path();
        }
        ImGui::SameLine();

        // Forward button
        if (ImGui::ArrowButton("##forward", ImGuiDir_Right))
        {
            // Could implement forward navigation
        }
        ImGui::SameLine();

        // Up button
        if (ImGui::Button(ICON_MD_ARROW_UPWARD))
        {
            if (m_currentPath.has_parent_path())
                m_currentPath = m_currentPath.parent_path();
        }
        ImGui::SameLine();

        // Path display
        std::string pathStr = m_currentPath.string();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 150);
        ImGui::InputText("##path", const_cast<char *>(pathStr.c_str()), ImGuiInputTextFlags_ReadOnly);

        ImGui::EndGroup();
    }

    void AssetBrowser::DrawFilterControls()
    {
        ImGui::BeginGroup();

        // Search filter with icon - Fixed InputTextWithHint call
        ImGui::SetNextItemWidth(250);
        static char searchBuffer[256] = {};
        ImGui::InputTextWithHint("##search", ICON_MD_SEARCH " Search assets...", searchBuffer, sizeof(searchBuffer));
        m_searchFilter = searchBuffer;

        ImGui::SameLine();

        // Type filter
        const char *typeNames[] = {
            "All", "Mesh", "Texture", "Shader", "LuaScript",
            "CppCode", "VFX", "Audio", "SkeletalAnimation", "Scene",
            "TemplateAsset", "ConfigFile", "Font", "Material", "Library",
            "AnimationStateMachine"};

        int currentType = static_cast<int>(m_typeFilter) + 1; // +1 because -1 = All
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("##typefilter", &currentType, typeNames, IM_ARRAYSIZE(typeNames)))
        {
            m_typeFilter = static_cast<AssetType>(currentType - 1);
        }

        ImGui::EndGroup();
    }

    void AssetBrowser::DrawAssetGrid()
    {
        auto controller = AssetController::Get();
        if (controller == nullptr)
            return;
        const auto &assets = controller->assets_;

        ImGui::BeginChild("AssetGrid", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        if (m_viewMode == ViewMode::List)
            DrawListView(assets);
        else
            DrawGridView(assets);

        // Right-click on empty grid space -> "New" menu
        if (ImGui::BeginPopupContextWindow("AssetBrowserContext",
                ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            DrawContextMenu();
            ImGui::EndPopup();
        }

        ImGui::EndChild();
    }

    void AssetBrowser::DrawGridView(const SFTL::DynamicArray<std::shared_ptr<AssetBase>> &assets)
    {
        float cellSize = m_thumbnailSize + 40.0f;
        float windowWidth = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>(windowWidth / cellSize));

        if (ImGui::BeginTable("AssetGrid", columns, ImGuiTableFlags_None))
        {
            int folderIndex = 0;
            if (std::filesystem::exists(m_currentPath) && std::filesystem::is_directory(m_currentPath))
            {
                std::error_code ec;
                for (const auto &entry : std::filesystem::directory_iterator(m_currentPath, ec))
                {
                    if (!entry.is_directory())
                        continue;

                    ImGui::TableNextColumn();
                    DrawFolderTile(entry.path(), folderIndex++);
                }
            }

            int itemIndex = 0;
            for (const auto &asset : assets)
            {
                if (!asset || !ShouldShowAsset(asset))
                    continue;

                ImGui::TableNextColumn();
                DrawAssetTile(asset, itemIndex++);
            }
            ImGui::EndTable();
        }
    }

    void AssetBrowser::DrawAssetTile(const std::shared_ptr<AssetBase> &asset, int index)
    {
        bool isSelected = m_selectedAsset && *m_selectedAsset == asset->uuid;

        ImGui::PushID(index);

        if (isSelected)
        {
            ImVec4 bgColor = ImGui::GetStyle().Colors[ImGuiCol_HeaderActive];
            bgColor.w = 0.3f;
            ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
        }

        ImGui::BeginGroup();

        ImVec2 thumbnailSize(m_thumbnailSize, m_thumbnailSize);
        bool renderedPreview = false;

        if (m_showThumbnails)
        {
            // No outer rtti_pointer_cast<ImageAsset> needed - TryWithImageTexture
            // already tries every concrete image type internally.
            TryWithImageTexture(asset, [&](auto &tex, const UUID &guid)
                                {
            auto preview = GetOrCreatePreview(guid, tex);
            if (preview.isValid && preview.textureID)
            {
                float aspect = static_cast<float>(preview.size.x) / static_cast<float>(preview.size.y);
                ImVec2 displaySize = thumbnailSize;
                if (aspect > 1.0f)
                    displaySize.y = thumbnailSize.x / aspect;
                else
                    displaySize.x = thumbnailSize.y * aspect;

                ImGui::Image(preview.textureID, displaySize);
                renderedPreview = true;
            } });
        }

        if (!renderedPreview)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  m_showThumbnails ? ImVec4(0.2f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 0.0f));
            if (ImGui::Button(("##" + std::to_string(index)).c_str(), thumbnailSize))
            {
                m_selectedAsset = asset->uuid;
            }
            ImGui::PopStyleColor();

            ImVec2 cursorPos = ImGui::GetCursorPos();
            ImGui::SetCursorPos(ImVec2(
                cursorPos.x + thumbnailSize.x / 2 - 16,
                cursorPos.y + thumbnailSize.y / 2 - 16));
            DrawAssetIcon(asset);
            ImGui::SetCursorPos(cursorPos);
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize.x);
        ImGui::TextWrapped("%s", asset->name.c_str()); // todo: click = rename, idk where and how FileSystem/File.hpp finna be used.
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();

        if (isSelected)
            ImGui::PopStyleColor();

        if (ImGui::BeginPopupContextItem("AssetContext"))
        {
            if (ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open"))
                m_selectedAsset = asset->uuid;
            if (ImGui::MenuItem(ICON_MD_DELETE " Delete"))
            {
                // Delete asset
            }
            if (ImGui::MenuItem(ICON_MD_SAVE " Save"))
                asset->Save();
            if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Copy UUID"))
                ImGui::SetClipboardText(asset->uuid.ToString().c_str());
            ImGui::EndPopup();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            m_selectedAsset = asset->uuid;

        ImGui::PopID();
    }

    void AssetBrowser::DrawListView(const SFTL::DynamicArray<std::shared_ptr<AssetBase>> &assets)
    {
        if (ImGui::BeginTable("AssetList", 5,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY))
        {
            ImGui::TableSetupColumn(ICON_MD_IMAGE " Preview", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn(ICON_MD_TITLE " Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(ICON_MD_CATEGORY " Type", ImGuiTableColumnFlags_WidthFixed, 150);
            ImGui::TableSetupColumn(ICON_MD_SETTINGS " Actions", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableHeadersRow();

            // Folders first
            if (std::filesystem::exists(m_currentPath) && std::filesystem::is_directory(m_currentPath))
            {
                std::error_code ec;
                for (const auto &entry : std::filesystem::directory_iterator(m_currentPath, ec))
                {
                    if (!entry.is_directory())
                        continue;

                    std::filesystem::path folderPath = entry.path();
                    std::string name = folderPath.filename().string();

                    ImGui::PushID(name.c_str());
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(ICON_MD_FOLDER);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TableSetColumnIndex(1);
                    bool isEditingThis = (m_inlineEditMode == InlineEditMode::RenameFolder && m_inlineEditPath == folderPath);
                    if (isEditingThis)
                    {
                        DrawFolderNameField(folderPath, -1); // -1 = fill remaining column width
                    }
                    else if (ImGui::Selectable(name.c_str(), false,
                                               ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        if (ImGui::IsMouseDoubleClicked(0))
                            m_currentPath = folderPath;
                    }

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("Folder");

                    ImGui::TableSetColumnIndex(3);
                    if (!isEditingThis)
                    {
                        if (ImGui::SmallButton(ICON_MD_DRIVE_FILE_RENAME_OUTLINE "##rename"))
                            BeginRenameFolder(folderPath);
                        ImGui::SameLine();
                        if (ImGui::SmallButton(ICON_MD_DELETE "##delete"))
                            RequestDeleteFolder(folderPath);
                    }

                    ImGui::PopID();
                }
            }

            for (const auto &asset : assets)
            {
                if (!asset || !ShouldShowAsset(asset))
                    continue;

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                DrawSmallPreview(asset);

                ImGui::TableSetColumnIndex(1);
                bool isSelected = m_selectedAsset && *m_selectedAsset == asset->uuid;

                if (ImGui::Selectable(asset->name.c_str(), isSelected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    m_selectedAsset = asset->uuid;
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", GetAssetTypeName(asset->type));

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton((ICON_MD_OPEN_IN_NEW "##open")))
                    m_selectedAsset = asset->uuid;
                ImGui::SameLine();
                if (ImGui::SmallButton((ICON_MD_SAVE "##save")))
                    asset->Save();
            }

            ImGui::EndTable();
        }
    }

    void AssetBrowser::DrawSmallPreview(const std::shared_ptr<AssetBase> &asset)
    {
        ImVec2 previewSize(50, 50);

        bool rendered = TryWithImageTexture(asset, [&](auto &tex, const UUID &guid)
                                            {
        auto preview = GetOrCreatePreview(guid, tex);
        if (preview.isValid && preview.textureID)
        {
            float aspect = static_cast<float>(preview.size.x) / static_cast<float>(preview.size.y);
            ImVec2 displaySize = previewSize;
            if (aspect > 1.0f)
                displaySize.y = previewSize.x / aspect;
            else
                displaySize.x = previewSize.y * aspect;

            ImGui::Image(preview.textureID, displaySize);
        } });

        if (rendered)
            return;

        // Fallback to icon
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
        std::string previewId = "##preview" + asset->uuid.ToString();
        ImGui::Button(previewId.c_str(), previewSize);
        ImGui::PopStyleColor();

        ImVec2 cursorPos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(
            cursorPos.x + previewSize.x / 2 - 12,
            cursorPos.y + previewSize.y / 2 - 12));
        DrawAssetIcon(asset);
        ImGui::SetCursorPos(cursorPos);
    }

    void AssetBrowser::DrawDetailsPanel()
    {
        ImGui::BeginChild("DetailsPanel", ImVec2(0, 0), true);

        auto asset = AssetController::Get()->FindByUUID(*m_selectedAsset);
        if (!asset)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                               ICON_MD_ERROR " Asset not found");
            ImGui::EndChild();
            return;
        }

        // Try different image types
        auto image2dAsset = ::SF::RTTI::rtti_pointer_cast<ImageAsset<Image2d>>(asset);
        auto image3dAsset = ::SF::RTTI::rtti_pointer_cast<ImageAsset<Image3d>>(asset);
        auto image2dArrayAsset = ::SF::RTTI::rtti_pointer_cast<ImageAsset<Image2dArray>>(asset);
        auto cubemapAsset = ::SF::RTTI::rtti_pointer_cast<ImageAsset<Cubemap>>(asset);

        if (image2dAsset)
            DrawImageDetails(image2dAsset);
        else if (image3dAsset)
            DrawImageDetails(image3dAsset);
        else if (image2dArrayAsset)
            DrawImageDetails(image2dArrayAsset);
        else if (cubemapAsset)
            DrawImageDetails(cubemapAsset);
        else
            DrawGenericAssetDetails(asset);

        ImGui::EndChild();
    }

    template <typename TImage>
    void AssetBrowser::DrawImageDetails(const std::shared_ptr<ImageAsset<TImage>> &asset)
    {
        ImGui::BeginGroup();

        // Header with icon
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f),
                           ICON_MD_IMAGE " Image Asset Details");
        ImGui::Separator();

        // Large preview
        if (asset->texture)
        {
            auto &tex = asset->texture;
            auto extent = tex->GetExtent();
            auto preview = GetOrCreatePreview(asset->uuid, tex);

            if (preview.isValid && preview.textureID)
            {
                ImGui::Text(ICON_MD_VISIBILITY " Preview:");

                float maxWidth = ImGui::GetContentRegionAvail().x - 20;
                float maxHeight = 300.0f;

                float aspect = static_cast<float>(preview.size.x) / static_cast<float>(preview.size.y);
                ImVec2 displaySize;

                if (aspect > 1.0f)
                {
                    displaySize.x = std::min(static_cast<float>(preview.size.x), maxWidth);
                    displaySize.y = displaySize.x / aspect;
                }
                else
                {
                    displaySize.y = std::min(static_cast<float>(preview.size.y), maxHeight);
                    displaySize.x = displaySize.y * aspect;
                }

                if (displaySize.x > maxWidth)
                {
                    displaySize.x = maxWidth;
                    displaySize.y = displaySize.x / aspect;
                }

                ImGui::Image(preview.textureID, displaySize);
                ImGui::Separator();
            }
        }

        // Properties in a grid
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), ICON_MD_INFO " Properties");

        if (ImGui::BeginTable("ImageProperties", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn(ICON_MD_LABEL " Property", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            AddPropertyRow(ICON_MD_TITLE " Name", asset->name);
            AddPropertyRow(ICON_MD_CATEGORY " Type", GetAssetTypeName(asset->type));

            if (asset->texture)
            {
                auto &tex = asset->texture;
                auto extent = tex->GetExtent();

                AddPropertyRow(ICON_MD_CROP_ORIGINAL " Dimensions",
                               std::to_string(extent.x) + "x" + std::to_string(extent.y) + "x" + std::to_string(extent.z));
                AddPropertyRow(ICON_MD_FORMAT_COLOR_FILL " Format", GetFormatName(tex->GetFormat()));
                AddPropertyRow(ICON_MD_LAYERS " Mip Levels", std::to_string(tex->GetMipLevels()));
                AddPropertyRow(ICON_MD_VIEW_COLUMN " Array Layers", std::to_string(tex->GetArrayLevels()));
                AddPropertyRow(ICON_MD_FILTER " Filter", GetFilterName(tex->GetFilter()));
                AddPropertyRow(ICON_MD_BORDER_ALL " Address Mode", GetAddressModeName(tex->GetAddressMode()));
                AddPropertyRow(ICON_MD_LABEL " Samples", std::to_string(tex->GetSamples()));
                AddPropertyRow(ICON_MD_SETTINGS " Usage", "0x" + std::to_string(tex->GetUsage()));
            }

            ImGui::EndTable();
        }

        ImGui::Separator();

        // Actions
        DrawActionButtons(asset);

        ImGui::EndGroup();
    }

    void AssetBrowser::DrawGenericAssetDetails(const std::shared_ptr<AssetBase> &asset)
    {
        ImGui::BeginGroup();

        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f),
                           ICON_MD_INFO " Asset Details");
        ImGui::Separator();

        if (ImGui::BeginTable("AssetProperties", 2,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn(ICON_MD_LABEL " Property", ImGuiTableColumnFlags_WidthFixed, 120);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            AddPropertyRow(ICON_MD_TITLE " Name", asset->name);
            AddPropertyRow(ICON_MD_CATEGORY " Type", GetAssetTypeName(asset->type));

            ImGui::EndTable();
        }

        ImGui::Separator();
        DrawActionButtons(asset);

        ImGui::EndGroup();
    }

    void AssetBrowser::AddPropertyRow(const std::string &label, const std::string &value)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", label.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%s", value.c_str());
    }

    template <typename T>
    void AssetBrowser::DrawActionButtons(const std::shared_ptr<T> &asset)
    {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f),
                           ICON_MD_SETTINGS " Actions");

        if (ImGui::Button(ICON_MD_SAVE " Save", ImVec2(100, 0)))
        {
            asset->Save();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_DELETE " Delete", ImVec2(100, 0)))
        {
            // Delete asset
        }
    }

    void AssetBrowser::DrawAssetIcon(const std::shared_ptr<AssetBase> &asset)
    {
        const char *icon = ICON_MD_INSERT_DRIVE_FILE;

        switch (asset->type)
        {
        case AssetType::Texture:
            icon = ICON_MD_IMAGE;
            break;
        case AssetType::Mesh:
            icon = ICON_MD_3D_ROTATION;
            break;
        case AssetType::Shader:
            icon = ICON_MD_CODE;
            break;
        case AssetType::Material:
            icon = ICON_MD_PALETTE;
            break;
        case AssetType::Audio:
            icon = ICON_MD_AUDIO_FILE;
            break;
        case AssetType::Scene:
            icon = ICON_MD_WEB;
            break;
        case AssetType::Font:
            icon = ICON_MD_FONT_DOWNLOAD;
            break;
        case AssetType::LuaScript:
        case AssetType::CppCode:
            icon = ICON_MD_TERMINAL;
            break;
        default:
            break;
        }

        ImGui::TextUnformatted(icon);
    }

    const char *AssetBrowser::GetAssetTypeName(AssetType type)
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

    const char *AssetBrowser::GetFormatName(VkFormat format)
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

    const char *AssetBrowser::GetFilterName(VkFilter filter)
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

    const char *AssetBrowser::GetAddressModeName(VkSamplerAddressMode mode)
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

    bool AssetBrowser::ShouldShowAsset(const std::shared_ptr<AssetBase> &asset)
    {
        // Check type filter
        if (m_typeFilter != static_cast<AssetType>(-1) && asset->type != m_typeFilter)
            return false;

        // Check search filter
        if (!m_searchFilter.empty())
        {
            std::string name = asset->name;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::string filter = m_searchFilter;
            std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

            if (name.find(filter) == std::string::npos)
                return false;
        }

        return true;
    }

    // TODO: AssetWizard class
    template <typename TImage>
    void AssetBrowser::CreateImageAsset()
    {
        static_assert(std::is_base_of_v<Image, TImage>,
                      "TImage must derive from Image");

        auto asset = AssetController::Get()->RegisterAsset<ImageAsset<TImage>>(
            "New" + std::string(GetAssetTypeName(AssetType::Texture)));

        // Create with appropriate dimensions based on image type
        if constexpr (std::is_same_v<TImage, Image3d>)
        {
            // Image3d needs 3D dimensions
            asset->Create(UVec3(256, 256, 256), VK_FORMAT_R8G8B8A8_UNORM);
        }
        else if constexpr (std::is_same_v<TImage, Cubemap>)
        {
            // Cubemap might have specific requirements - check your implementation
            // For now, use 2D dimensions as it's likely a square texture
            asset->Create(UVec2(256, 256), VK_FORMAT_R8G8B8A8_UNORM);
        }
        else
        {
            // Image2d and Image2dArray use 2D dimensions
            asset->Create(UVec2(256, 256), VK_FORMAT_R8G8B8A8_UNORM);
        }

        // Save the asset
        asset->Save();

        // Select the new asset
        m_selectedAsset = asset->uuid;
    }

    void AssetBrowser::RefreshAssets()
    {
        m_previewCache.clear();
    }

    // Helper to get or create texture preview
    template <typename TImage>
    AssetBrowser::TexturePreview AssetBrowser::GetOrCreatePreview(const UUID &guid, const std::shared_ptr<TImage> &texture)
    {
        auto it = m_previewCache.find(guid);
        if (it != m_previewCache.end() && it->second.isValid)
            return it->second;

        TexturePreview preview;
        preview.isValid = false;
        preview.size = texture->GetExtent();

        // Here you would create an ImGui texture ID from your Vulkan image
        // This is a placeholder - you'd need to integrate with your rendering system
        // to create ImGui-compatible texture IDs

        // For example with your Vulkan backend:
        // preview.textureID = ImGui_ImplVulkan_AddTexture(texture->GetSampler(), texture->GetView(), texture->GetLayout());

        m_previewCache[guid] = preview;
        return preview;
    }

    void AssetBrowser::DrawFolderNameField(const std::filesystem::path &folderPath, float width)
    {
        ImGui::SetNextItemWidth(width);

        if (m_inlineEditFocusPending)
        {
            ImGui::SetKeyboardFocusHere();
            m_inlineEditFocusPending = false;
        }

        bool confirmed = ImGui::InputText("##inlinefoldername", m_inlineEditBuffer, sizeof(m_inlineEditBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

        if (confirmed)
            CommitInlineFolderRename();
        else if (ImGui::IsItemDeactivated()) // clicked away without pressing Enter -> commit, like Unity
            CommitInlineFolderRename();
    }
    void AssetBrowser::DrawFolderTile(const std::filesystem::path &folderPath, int index)
    {
        std::string name = folderPath.filename().string();
        bool isEditingThis = (m_inlineEditMode == InlineEditMode::RenameFolder && m_inlineEditPath == folderPath);

        ImGui::PushID(("folder_" + std::to_string(index)).c_str());
        ImGui::BeginGroup();

        ImVec2 thumbnailSize(m_thumbnailSize, m_thumbnailSize);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.3f, 0.15f, 1.0f));
        ImGui::Button("##folderbtn", thumbnailSize);
        ImGui::PopStyleColor();

        ImVec2 cursorPos = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(
            cursorPos.x + thumbnailSize.x / 2 - 16,
            cursorPos.y - thumbnailSize.y / 2 - 16));
        ImGui::TextUnformatted(ICON_MD_FOLDER);
        ImGui::SetCursorPos(cursorPos);

        if (isEditingThis)
        {
            DrawFolderNameField(folderPath, thumbnailSize.x);
        }
        else
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + thumbnailSize.x);
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::PopTextWrapPos();
        }

        ImGui::EndGroup();

        if (!isEditingThis && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            m_currentPath = folderPath;

        if (!isEditingThis && ImGui::BeginPopupContextItem("FolderContext"))
        {
            if (ImGui::MenuItem(ICON_MD_OPEN_IN_NEW " Open"))
                m_currentPath = folderPath;
            ImGui::Separator();
            if (ImGui::MenuItem(ICON_MD_DRIVE_FILE_RENAME_OUTLINE " Rename"))
                BeginRenameFolder(folderPath);
            if (ImGui::MenuItem(ICON_MD_DELETE " Delete"))
                RequestDeleteFolder(folderPath);
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    std::filesystem::path AssetBrowser::CreateUniqueFolder(const std::filesystem::path &parent, const std::string &baseName)
    {
        std::filesystem::path candidate = parent / baseName;
        int suffix = 1;
        while (File::Exists(candidate.string()))
            candidate = parent / (baseName + " (" + std::to_string(suffix++) + ")");
        return candidate;
    }

    void AssetBrowser::BeginRenameFolder(const std::filesystem::path &path)
    {
        m_inlineEditMode = InlineEditMode::RenameFolder;
        m_inlineEditPath = path;
        std::snprintf(m_inlineEditBuffer, sizeof(m_inlineEditBuffer), "%s", path.filename().string().c_str());
        m_inlineEditFocusPending = true;
    }

    void AssetBrowser::CommitInlineFolderRename()
    {
        std::string newName = m_inlineEditBuffer;
        while (!newName.empty() && std::isspace(static_cast<unsigned char>(newName.back())))
            newName.pop_back();

        if (!newName.empty() && newName != m_inlineEditPath.filename().string())
        {
            std::filesystem::path newPath = m_inlineEditPath.parent_path() / newName;
            if (!File::Exists(newPath.string()) &&
                File::Rename(m_inlineEditPath.string(), newPath.string()))
            {
                if (m_currentPath == m_inlineEditPath)
                    m_currentPath = newPath;
            }
        }

        m_inlineEditMode = InlineEditMode::None;
        m_inlineEditPath.clear();
    }

    void AssetBrowser::CancelInlineFolderEdit()
    {
        m_inlineEditMode = InlineEditMode::None;
        m_inlineEditPath.clear();
    }

    void AssetBrowser::RequestDeleteFolder(const std::filesystem::path &path)
    {
        m_deleteTargetPath = path;
        m_showDeleteConfirmPopup = true;
    }

    void AssetBrowser::DrawDeleteFolderConfirmPopup()
    {
        if (m_showDeleteConfirmPopup)
        {
            ImGui::OpenPopup("Delete Folder");
            m_showDeleteConfirmPopup = false;
        }

        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Delete Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), ICON_MD_WARNING " Delete this folder and all its contents?");
            ImGui::TextWrapped("%s", m_deleteTargetPath.string().c_str());
            ImGui::Separator();

            if (ImGui::Button(ICON_MD_DELETE " Delete", ImVec2(120, 0)))
            {
                bool wasCurrentOrParent =
                    m_currentPath == m_deleteTargetPath ||
                    (m_currentPath.string().rfind(m_deleteTargetPath.string(), 0) == 0);

                File::DeleteDirectory(m_deleteTargetPath.string(), true);

                if (wasCurrentOrParent && m_deleteTargetPath.has_parent_path())
                    m_currentPath = m_deleteTargetPath.parent_path();

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(ICON_MD_CLOSE " Cancel", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void AssetBrowser::DrawContextMenu()
    {
        if (ImGui::BeginMenu("New"))
        {
            // BASIC
            if (ImGui::MenuItem("Folder"))
            {
                auto uniquePath = CreateUniqueFolder(m_currentPath, "New Folder");
                if (File::CreateDirectory(uniquePath.string()))
                    BeginRenameFolder(uniquePath);
            }
            if (ImGui::MenuItem("Prefab"))
            {
                // ShowPrefabWizzard();
            }

            ImGui::Separator();
            // SHADERS
            if (ImGui::MenuItem("Shader"))
            {
                showCS = true;
            }
            if (ImGui::MenuItem("Shader Include"))
            {
                showCSI = true;
            }
            // Script
            if (ImGui::BeginMenu("Script"))
            {
                if (ImGui::MenuItem("C++ Header"))
                {
                }
                if (ImGui::MenuItem("C++ Source"))
                {
                }
                if (ImGui::MenuItem("Lua Source"))
                {
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
    }
}