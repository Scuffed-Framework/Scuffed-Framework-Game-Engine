#include "Project.hpp"
#include <Assets/AssetPipeline.hpp>
#include <Gui/ImGui/GuiMembers.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
#include <numeric>

namespace SF::Engine
{
    namespace
    {
        constexpr int ProjectFileVersion = 1;
        enum class Mode
        {
            Open,
            Create
        };

        Mode s_mode         = Mode::Open;
        int s_selectedIndex = -1;

        char s_newName[256]   = "";
        char s_newFolder[512] = "";
        char s_newDesc[1024]  = "";

        std::vector<ProjectLoadInfo> s_recentProjects;
        bool s_recentLoaded = false;

        std::vector<ProjectTemplate> s_templates;
        bool s_templatesLoaded = false;

        std::string s_statusMsg;
        float s_statusTimer             = 0.0f;
        constexpr float kStatusDuration = 3.0f; // seconds

        void SetStatus(const char *msg)
        {
            s_statusMsg   = msg;
            s_statusTimer = kStatusDuration;
        }

        void TickStatus(float dt)
        {
            if (s_statusTimer > 0.0f)
                s_statusTimer -= dt;
        }

        void EnsureRecentProjectsLoaded()
        {
            if (s_recentLoaded)
                return;
            // TODO: read from a registry / xml side-car file on disk.
            s_recentLoaded = true;
        }

        void EnsureTemplatesLoaded()
        {
            if (s_templatesLoaded)
                return;
            // TODO: scan engine templates directory and populate s_templates.
            s_templatesLoaded = true;
        }

        // Draw a preview image, or a grey placeholder when img == nullptr.
        void DrawPreviewImage(const Image2d *img, Vec2 size)
        {
            Vec2 cursor    = ImGui::GetCursorScreenPos();
            ImDrawList *dl = ImGui::GetWindowDrawList();

            if (img)
            {
                // Wire up texture handle here:
                // ImGui::Image((ImTextureID)(intptr_t)img->GetTextureID(), size);
                // For now draw a tinted placeholder so it looks distinct.
                dl->AddRectFilled(cursor, {cursor.x + size.x, cursor.y + size.y}, IM_COL32(30, 50, 70, 255));
                dl->AddRect(cursor, {cursor.x + size.x, cursor.y + size.y}, IM_COL32(80, 140, 200, 255));
                const char *lbl = "Preview";
                Vec2 lsize      = ImGui::CalcTextSize(lbl);
                dl->AddText({cursor.x + (size.x - lsize.x) * 0.5f, cursor.y + (size.y - lsize.y) * 0.5f},
                            IM_COL32(80, 140, 200, 255), lbl);
            } else
            {
                dl->AddRectFilled(cursor, {cursor.x + size.x, cursor.y + size.y}, IM_COL32(45, 45, 45, 255));
                dl->AddRect(cursor, {cursor.x + size.x, cursor.y + size.y}, IM_COL32(100, 100, 100, 255));
                const char *lbl = "No Preview";
                Vec2 lsize      = ImGui::CalcTextSize(lbl);
                dl->AddText({cursor.x + (size.x - lsize.x) * 0.5f, cursor.y + (size.y - lsize.y) * 0.5f},
                            IM_COL32(130, 130, 130, 255), lbl);
            }
            ImGui::Dummy(size);
        }

        // Returns true on double-click (caller should treat as "confirm").
        bool SelectableItem(const std::string &label, bool selected, int index)
        {
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                s_selectedIndex = index;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    return true;
            }
            return false;
        }

        // Translate a ProjectResult to a human-readable string.
        const char *ResultString(ProjectResult r)
        {
            switch (r)
            {
                case ProjectResult::Success:
                    return "Success.";
                case ProjectResult::NotFound:
                    return "Error: file not found.";
                case ProjectResult::InvalidFormat:
                    return "Error: invalid project format.";
                case ProjectResult::VersionMismatch:
                    return "Error: version mismatch.";
                default:
                    return "Error: unknown failure.";
            }
        }
    } // namespace

    template class ModuleRegistrar<ProjectManager>; // idk why


    ProjectResult ProjectManager::SaveProject() const
    {
        if (!currentLoadedProject)
            return ProjectResult::NotFound;

        std::error_code ec;
        std::filesystem::create_directories(currentLoadedProject->Path.parent_path(), ec);
        if (ec)
            return ProjectResult::UnknownError;

        auto *xml = XMLModule::Get();
        xml->SetRootNode("SFProject");
        XMLNode root = xml->GetRootNode();
        root.SetAttribute("version", ProjectFileVersion);
        currentLoadedProject->Serialize(root);

        return xml->SaveToFile(currentLoadedProject->Path.string()) ? ProjectResult::Success
                                                                    : ProjectResult::UnknownError;
    }


    // ProjectManager::CreateProject
    //
    // Directory layout created on disk:
    //   <path>/
    //     <name>.projxml     <- project XML descriptor

    ProjectResult ProjectManager::CreateProject(const std::string &name, const std::filesystem::path &path,
                                                const std::string &desc)
    {
        namespace fs        = std::filesystem;
        const fs::path root = path / name; // `path` is the parent folder chosen in the dialog

        std::error_code ec;
        for (const char *sub: {"Assets", "Logs", "Cache", "Build"})
        {
            fs::create_directories(root / sub, ec);
            if (ec)
                return ProjectResult::UnknownError;
        }

        auto project         = std::make_unique<Project>();
        project->name        = name;
        project->description = desc;
        project->Path        = root / (name + ProjectFileExtension);

        auto previous        = std::move(currentLoadedProject);
        currentLoadedProject = std::move(project);

        const ProjectResult r = SaveProject();
        if (r != ProjectResult::Success)
            currentLoadedProject = std::move(previous); // don't leave an unsaved project active
        return r;
    }


    ProjectResult ProjectManager::LoadProject(const std::filesystem::path &path)
    {
        if (!std::filesystem::exists(path))
            return ProjectResult::NotFound;

        auto *xml = XMLModule::Get();
        if (!xml->LoadFromFile(path.string()))
            return ProjectResult::InvalidFormat;

        XMLNode root = xml->GetRootNode();
        if (!root.IsValid() || !root.GetChild("Project").IsValid())
            return ProjectResult::InvalidFormat;

        int version = 0;
        if (!root.GetAttribute("version", version) || version != ProjectFileVersion)
            return ProjectResult::VersionMismatch;

        auto project = std::make_unique<Project>();
        project->Deserialize(root);
        project->Path        = path; // trust where we actually opened it from, not the stored path
        currentLoadedProject = std::move(project);
        return ProjectResult::Success;
    }

    void ProjectManager::DrawProjectManagerWindow()
    {
        if (projectWindowOpen)
        {

            EnsureRecentProjectsLoaded();
            EnsureTemplatesLoaded();

            // Tick status timer using ImGui's delta time.
            TickStatus(ImGui::GetIO().DeltaTime);
            auto Finish = [this](ProjectResult r)
            {
                SetStatus(ResultString(r));
                if (r == ProjectResult::Success)
                {
                    projectWindowOpen = false;
                    s_selectedIndex   = -1;
                }
                return r;
            };

            ImGuiIO &io = ImGui::GetIO();
            ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Once, {0.5f, 0.5f});
            ImGui::SetNextWindowSize({860.0f, 520.0f}, ImGuiCond_Once);
            ImGui::SetNextWindowSizeConstraints({640.0f, 400.0f}, {1400.0f, 900.0f});

            if (!ImGui::Begin("Project Manager##PMWin", &projectWindowOpen, ImGuiWindowFlags_NoCollapse))
            {
                ImGui::End();
                return;
            }

            const float totalW  = ImGui::GetContentRegionAvail().x;
            const float totalH  = ImGui::GetContentRegionAvail().y;
            const float listW   = totalW * 0.60f;
            const float detailW = totalW - listW - ImGui::GetStyle().ItemSpacing.x;
            // Footer: tab buttons row + optional status row
            const float footerH    = ImGui::GetFrameHeightWithSpacing() * 2.0f + 8.0f;
            const float listH      = totalH - footerH;
            const Vec2 previewSize = {detailW, detailW * 0.5625f}; // 16:9

            ImGui::BeginChild("##LeftPanel", {listW, listH}, false);
            {
                ImGui::TextDisabled("(Alphabetical Order)");
                ImGui::Separator();

                ImGui::BeginChild("##ItemList", {0.0f, 0.0f}, true);
                {
                    if (s_mode == Mode::Open)
                    {
                        if (s_recentProjects.empty())
                        {
                            ImGui::TextDisabled("No recent projects found.");
                        } else
                        {
                            std::vector<int> sorted(s_recentProjects.size());
                            std::iota(sorted.begin(), sorted.end(), 0);
                            ranges::sort(sorted, [](int a, int b)
                                         { return s_recentProjects[a].name < s_recentProjects[b].name; });

                            for (int idx: sorted)
                            {
                                const auto &proj = s_recentProjects[idx];
                                ImGui::PushID(idx);

                                if (SelectableItem(proj.name, s_selectedIndex == idx, idx))
                                {
                                    // Double-click: open immediately.
                                    if (!proj.projectPath.empty())
                                    {
                                        Finish(LoadProject(proj.projectPath));
                                    }
                                }

                                // Show path as a dim sub-line when selected.
                                if (s_selectedIndex == idx)
                                {
                                    ImGui::Indent();
                                    ImGui::TextDisabled("%s", proj.projectPath.string().c_str());
                                    ImGui::Unindent();
                                }

                                ImGui::PopID();
                            }
                        }
                    } else // Mode::Create – template list
                    {
                        if (s_templates.empty())
                        {
                            ImGui::TextDisabled("No templates available.");
                        } else
                        {
                            std::vector<int> sorted(s_templates.size());
                            std::iota(sorted.begin(), sorted.end(), 0);
                            ranges::sort(sorted,
                                         [](int a, int b) { return s_templates[a].name < s_templates[b].name; });

                            for (int idx: sorted)
                            {
                                ImGui::PushID(idx);
                                SelectableItem(s_templates[idx].name, s_selectedIndex == idx, idx);
                                ImGui::PopID();
                            }
                        }
                    }
                }
                ImGui::EndChild(); // ##ItemList
            }
            ImGui::EndChild(); // ##LeftPanel

            ImGui::SameLine();
            ImGui::BeginChild("##RightPanel", {detailW, listH}, false);
            {
                if (s_mode == Mode::Open)
                {
                    const Image2d *img =
                            (s_selectedIndex >= 0 && s_selectedIndex < static_cast<int>(s_recentProjects.size()))
                                    ? s_recentProjects[s_selectedIndex].aFrame.get()
                                    : nullptr;
                    DrawPreviewImage(img, previewSize);

                    ImGui::Spacing();
                    if (s_selectedIndex >= 0 && s_selectedIndex < static_cast<int>(s_recentProjects.size()))
                    {
                        const auto &p = s_recentProjects[s_selectedIndex];
                        ImGui::TextWrapped("Name:  %s", p.name.c_str());
                        ImGui::TextWrapped("Path:  %s", p.projectPath.string().c_str());
                        if (!p.description.empty())
                        {
                            ImGui::Spacing();
                            ImGui::TextWrapped("%s", p.description.c_str());
                        }
                    } else
                    {
                        ImGui::TextDisabled("Select a project to see details.");
                    }
                    if (ImGui::Button("Browse"))
                    {
                        IGFD::FileDialogConfig cfg;
                        cfg.path  = ".";
                        cfg.flags = ImGuiFileDialogFlags_Modal;
                        ImGuiFileDialog::Instance()->OpenDialog("FindProjDir", "Find Project",
                                                                ProjectFileExtension.data(), cfg);
                    }
                    if (ImGuiFileDialog::Instance()->Display("FindProjDir", ImGuiWindowFlags_NoCollapse,
                                                             Vec2(700, 500)))
                    {
                        if (ImGuiFileDialog::Instance()->IsOk())
                            Finish(LoadProject(
                                    ImGuiFileDialog::Instance()->GetFilePathName())); // the file, not the folder
                        ImGuiFileDialog::Instance()->Close();
                    }
                } else // Mode::Create
                {
                    const Image2d *img =
                            (s_selectedIndex >= 0 && s_selectedIndex < static_cast<int>(s_templates.size()))
                                    ? s_templates[s_selectedIndex].ExampleImage.get()
                                    : nullptr;
                    DrawPreviewImage(img, previewSize);

                    ImGui::Spacing();
                    if (s_selectedIndex >= 0 && s_selectedIndex < static_cast<int>(s_templates.size()))
                        ImGui::TextWrapped("%s", s_templates[s_selectedIndex].description.c_str());

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // --- Name ---
                    ImGui::SetNextItemWidth(-1.0f);
                    InputTextWithHint("##ProjName", "Project Name *", s_newName, sizeof(s_newName));

                    ImGui::Spacing();

                    // --- Folder + Browse ---
                    const float browseW =
                            ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
                    ImGui::SetNextItemWidth(-browseW - ImGui::GetStyle().ItemSpacing.x);
                    InputTextWithHint("##ProjFolder", "Project Folder *", s_newFolder, sizeof(s_newFolder));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse"))
                    {
                        IGFD::FileDialogConfig cfg;
                        cfg.path  = ".";
                        cfg.flags = ImGuiFileDialogFlags_Modal;
                        ImGui::SetNextWindowSize(Vec2(700, 500));
                        ImGuiFileDialog::Instance()->OpenDialog("ChooseProjDir", "Choose Project Folder", nullptr, cfg);
                    }

                    // Handle ImGuiFileDialog result.
                    if (ImGuiFileDialog::Instance()->Display("ChooseProjDir", ImGuiWindowFlags_NoCollapse,
                                                             Vec2(700, 500)))
                    {
                        if (ImGuiFileDialog::Instance()->IsOk())
                        {
                            std::string chosen = ImGuiFileDialog::Instance()->GetCurrentPath();
                            std::strncpy(s_newFolder, chosen.c_str(), sizeof(s_newFolder) - 1);
                            s_newFolder[sizeof(s_newFolder) - 1] = '\0';
                        }
                        ImGuiFileDialog::Instance()->Close();
                    }

                    ImGui::Spacing();

                    // --- Description ---
                    ImGui::SetNextItemWidth(-1.0f);
                    ImGui::InputTextMultiline("##ProjDesc", s_newDesc, sizeof(s_newDesc),
                                              {-1.0f, ImGui::GetTextLineHeight() * 4.0f});
                    if (s_newDesc[0] == '\0' && !ImGui::IsItemActive())
                    {
                        ImDrawList *dl = ImGui::GetWindowDrawList();
                        Vec2 pos       = ImGui::GetItemRectMin();
                        pos.x += ImGui::GetStyle().FramePadding.x;
                        pos.y += ImGui::GetStyle().FramePadding.y;
                        dl->AddText(pos, ImGui::GetColorU32(ImGuiCol_TextDisabled), "Description (optional)");
                    }
                }
            }
            ImGui::EndChild(); // ##RightPanel

            ImGui::Separator();
            ImGui::Spacing();

            // -- Mode tabs (left side) --
            auto StyledTabButton = [](const char *label, bool active) -> bool
            {
                if (active)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                }
                bool clicked = ImGui::Button(label);
                if (active)
                    ImGui::PopStyleColor(2);
                return clicked;
            };

            if (StyledTabButton("Open Project", s_mode == Mode::Open) && s_mode != Mode::Open)
            {
                s_mode          = Mode::Open;
                s_selectedIndex = -1;
            }

            ImGui::SameLine(0.0f, 4.0f);

            if (StyledTabButton("Create Project", s_mode == Mode::Create) && s_mode != Mode::Create)
            {
                s_mode          = Mode::Create;
                s_selectedIndex = -1;
                s_newName[0]    = '\0';
                s_newFolder[0]  = '\0';
                s_newDesc[0]    = '\0';
            }

            // -- Confirm button (right side) --
            const char *actionLabel = (s_mode == Mode::Open) ? "Open" : "Create";
            const float actionW = ImGui::CalcTextSize(actionLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
            ImGui::SameLine(totalW - actionW);

            const bool canAct =
                    (s_mode == Mode::Open) ? (s_selectedIndex >= 0) : (s_newName[0] != '\0' && s_newFolder[0] != '\0');

            if (!canAct)
                ImGui::BeginDisabled();

            if (ImGui::Button(actionLabel))
            {
                if (s_mode == Mode::Open)
                {
                    if (s_selectedIndex >= 0 && s_selectedIndex < static_cast<int>(s_recentProjects.size()))
                        Finish(LoadProject(s_recentProjects[s_selectedIndex].projectPath));
                } else
                {
                    // pass the PARENT folder; CreateProject makes <folder>/<name>/
                    if (Finish(CreateProject(s_newName, s_newFolder, s_newDesc)) == ProjectResult::Success)
                    {
                        s_newName[0] = s_newFolder[0] = s_newDesc[0] = '\0';
                        s_mode                                       = Mode::Open;
                    }
                }
            }

            if (!canAct)
                ImGui::EndDisabled();

            if (s_statusTimer > 0.0f)
            {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 80, 255));
                ImGui::TextUnformatted(s_statusMsg.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::End();
        }
    }

    bool ProjectManager::Initialize()
    {
        UIRegistry::Get().Register([this] { DrawProjectManagerWindow(); });
        return true;
    }

    void ProjectManager::Update() {}

} // namespace SF::Engine
