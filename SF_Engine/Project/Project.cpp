#include "Project.hpp"
#include <functional>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <GUI/GuiMembers.hpp>

namespace SF::Engine
{
    template class ModuleRegistrar<ProjectManager>; // idk why

    // ProjectManager::CreateProject
    //
    // Directory layout created on disk:
    //   <path>/
    //     <name>.projxml     ← project XML descriptor

    ProjectResult ProjectManager::CreateProject(const std::string &name,
                                                const std::filesystem::path &path)
    {
        if (name.empty())
            return ProjectResult::InvalidFormat;

        // 1. Create the directory.
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        std::filesystem::create_directories(path / "Assets", ec);
        std::filesystem::create_directories(path / "Logs", ec);
        std::filesystem::create_directories(path / "Cache", ec);
        std::filesystem::create_directories(path / "Build", ec);

        if (ec)
            return ProjectResult::UnknownError;

        std::filesystem::path xmlPath = path / (name + ".projxml");
        if (std::filesystem::exists(xmlPath))
            return ProjectResult::InvalidFormat; // already exists

        // 2. Build the XML document and serialise into it.
        XMLModule* writer = XMLModule::Get();
        writer->SetRootNode("Project"); // creates the xmlDoc + root element
        XMLNode root = writer->GetRootNode();

        auto proj = std::make_unique<Project>();
        proj->name = name;
        proj->Path = xmlPath;
        proj->Serialize(root); // writes name + projectFilePath attributes

        // 3. Save to disk – XMLModule handles everything, no File needed.
        if (!writer->SaveToFile(xmlPath.string()))
            return ProjectResult::UnknownError;

        proj->projectXML = File(xmlPath); // keep the File handle for later use

        // 4. Register in recent list and make active.
        ProjectLoadeInfo info;
        info.name = name;
        info.projectPath = xmlPath;
        s_recentProjects.insert(s_recentProjects.begin(), std::move(info));

        delete currentLoadedProject;
        currentLoadedProject = proj.release();
        projectWindowOpen = false;
        return ProjectResult::Success;
    }

    ProjectResult ProjectManager::LoadProject(const std::filesystem::path &path)
    {
        // Resolve directory → <dirname>.projxml fallback.
        std::filesystem::path xmlPath = path;
        if (std::filesystem::is_directory(xmlPath))
            xmlPath = xmlPath / (xmlPath.filename().string() + ".projxml");

        if (!std::filesystem::exists(xmlPath))
            return ProjectResult::NotFound;

        // 1. Parse the XML file.
        XMLModule* reader = XMLModule::Get();
        if (!reader->LoadFromFile(xmlPath.string()))
            return ProjectResult::InvalidFormat;

        XMLNode root = reader->GetRootNode();
        if (root.GetName() != "Project")
            return ProjectResult::InvalidFormat;

        // 2. Deserialise into a new Project.
        auto proj = std::make_unique<Project>();
        proj->projectXML = File(xmlPath);
        proj->Deserialize(root); // reads name + projectFilePath

        if (proj->name.empty())
            return ProjectResult::InvalidFormat;

        // 3. Update recent-projects list (bubble to front or insert).
        auto it = std::find_if(s_recentProjects.begin(), s_recentProjects.end(),
                               [&](const ProjectLoadeInfo &p)
                               { return p.projectPath == xmlPath; });
        if (it == s_recentProjects.end())
        {
            ProjectLoadeInfo info;
            info.name = proj->name;
            info.projectPath = xmlPath;
            s_recentProjects.insert(s_recentProjects.begin(), std::move(info));
        }
        else
        {
            std::rotate(s_recentProjects.begin(), it, it + 1);
        }

        // 4. Swap in the loaded project.
        delete currentLoadedProject;
        currentLoadedProject = proj.release();
        projectWindowOpen = false;
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

            ImGuiIO &io = ImGui::GetIO();
            ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f},
                                    ImGuiCond_Once, {0.5f, 0.5f});
            ImGui::SetNextWindowSize({860.0f, 520.0f}, ImGuiCond_Once);
            ImGui::SetNextWindowSizeConstraints({640.0f, 400.0f}, {1400.0f, 900.0f});

            if (!ImGui::Begin("Project Manager##PMWin", nullptr, ImGuiWindowFlags_NoCollapse))
            {
                ImGui::End();
                return;
            }

            const float totalW = ImGui::GetContentRegionAvail().x;
            const float totalH = ImGui::GetContentRegionAvail().y;
            const float listW = totalW * 0.60f;
            const float detailW = totalW - listW - ImGui::GetStyle().ItemSpacing.x;
            // Footer: tab buttons row + optional status row
            const float footerH = ImGui::GetFrameHeightWithSpacing() * 2.0f + 8.0f;
            const float listH = totalH - footerH;
            const ImVec2 previewSize = {detailW, detailW * 0.5625f}; // 16:9

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
                        }
                        else
                        {
                            std::vector<int> sorted(s_recentProjects.size());
                            std::iota(sorted.begin(), sorted.end(), 0);
                            std::sort(sorted.begin(), sorted.end(), [](int a, int b)
                                      { return s_recentProjects[a].name < s_recentProjects[b].name; });

                            for (int idx : sorted)
                            {
                                const auto &proj = s_recentProjects[idx];
                                ImGui::PushID(idx);

                                if (SelectableItem(proj.name, s_selectedIndex == idx, idx))
                                {
                                    // Double-click: open immediately.
                                    if (!proj.projectPath.empty())
                                    {
                                        auto r = LoadProject(proj.projectPath);
                                        SetStatus(ResultString(r));
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
                    }
                    else // Mode::Create – template list
                    {
                        if (s_templates.empty())
                        {
                            ImGui::TextDisabled("No templates available.");
                        }
                        else
                        {
                            std::vector<int> sorted(s_templates.size());
                            std::iota(sorted.begin(), sorted.end(), 0);
                            std::sort(sorted.begin(), sorted.end(), [](int a, int b)
                                      { return s_templates[a].name < s_templates[b].name; });

                            for (int idx : sorted)
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
                        (s_selectedIndex >= 0 && s_selectedIndex < (int)s_recentProjects.size())
                            ? s_recentProjects[s_selectedIndex].aFrame.get()
                            : nullptr;
                    DrawPreviewImage(img, previewSize);

                    ImGui::Spacing();
                    if (s_selectedIndex >= 0 && s_selectedIndex < (int)s_recentProjects.size())
                    {
                        const auto &p = s_recentProjects[s_selectedIndex];
                        ImGui::TextWrapped("Name:  %s", p.name.c_str());
                        ImGui::TextWrapped("Path:  %s", p.projectPath.string().c_str());
                        if (!p.description.empty())
                        {
                            ImGui::Spacing();
                            ImGui::TextWrapped("%s", p.description.c_str());
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("Select a project to see details.");
                    }
                    if (ImGui::Button("Browse"))
                    {
                        IGFD::FileDialogConfig cfg;
                        cfg.path = ".";
                        cfg.flags = ImGuiFileDialogFlags_Modal;
                        ImGui::SetNextWindowSize(ImVec2(700, 500));
                        ImGuiFileDialog::Instance()->OpenDialog(
                            "FindProjDir", "Find Project", ".projxml", cfg);
                    }
                    // Handle ImGuiFileDialog result.
                    if (ImGuiFileDialog::Instance()->Display("FindProjDir"))
                    {
                        if (ImGuiFileDialog::Instance()->IsOk())
                        {
                            std::string chosen = ImGuiFileDialog::Instance()->GetCurrentPath();
                            LoadProject(chosen);
                        }
                        ImGuiFileDialog::Instance()->Close();
                    }
                }
                else // Mode::Create
                {
                    const Image2d *img =
                        (s_selectedIndex >= 0 && s_selectedIndex < (int)s_templates.size())
                            ? s_templates[s_selectedIndex].ExampleImage.get()
                            : nullptr;
                    DrawPreviewImage(img, previewSize);

                    ImGui::Spacing();
                    if (s_selectedIndex >= 0 && s_selectedIndex < (int)s_templates.size())
                        ImGui::TextWrapped("%s", s_templates[s_selectedIndex].description.c_str());

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    // --- Name ---
                    ImGui::SetNextItemWidth(-1.0f);
                    InputTextWithHint("##ProjName", "Project Name *", s_newName, sizeof(s_newName));

                    ImGui::Spacing();

                    // --- Folder + Browse ---
                    const float browseW = ImGui::CalcTextSize("Browse").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
                    ImGui::SetNextItemWidth(-browseW - ImGui::GetStyle().ItemSpacing.x);
                    InputTextWithHint("##ProjFolder", "Project Folder *", s_newFolder, sizeof(s_newFolder));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse"))
                    {
                        IGFD::FileDialogConfig cfg;
                        cfg.path = ".";
                        cfg.flags = ImGuiFileDialogFlags_Modal;
                        ImGui::SetNextWindowSize(ImVec2(700, 500));
                        ImGuiFileDialog::Instance()->OpenDialog(
                            "ChooseProjDir", "Choose Project Folder", nullptr, cfg);
                    }

                    // Handle ImGuiFileDialog result.
                    if (ImGuiFileDialog::Instance()->Display("ChooseProjDir"))
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
                        ImVec2 pos = ImGui::GetItemRectMin();
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
                    ImGui::PushStyleColor(ImGuiCol_Button,
                                          ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                          ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                }
                bool clicked = ImGui::Button(label);
                if (active)
                    ImGui::PopStyleColor(2);
                return clicked;
            };

            if (StyledTabButton("Open Project", s_mode == Mode::Open) && s_mode != Mode::Open)
            {
                s_mode = Mode::Open;
                s_selectedIndex = -1;
            }

            ImGui::SameLine(0.0f, 4.0f);

            if (StyledTabButton("Create Project", s_mode == Mode::Create) && s_mode != Mode::Create)
            {
                s_mode = Mode::Create;
                s_selectedIndex = -1;
                s_newName[0] = '\0';
                s_newFolder[0] = '\0';
                s_newDesc[0] = '\0';
            }

            // -- Confirm button (right side) --
            const char *actionLabel = (s_mode == Mode::Open) ? "Open" : "Create";
            const float actionW = ImGui::CalcTextSize(actionLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
            ImGui::SameLine(totalW - actionW);

            const bool canAct = (s_mode == Mode::Open)
                                    ? (s_selectedIndex >= 0)
                                    : (s_newName[0] != '\0' && s_newFolder[0] != '\0');

            if (!canAct)
                ImGui::BeginDisabled();

            if (ImGui::Button(actionLabel))
            {
                ProjectResult result = ProjectResult::UnknownError;

                if (s_mode == Mode::Open)
                {
                    if (s_selectedIndex >= 0 && s_selectedIndex < (int)s_recentProjects.size())
                        result = LoadProject(s_recentProjects[s_selectedIndex].projectPath);
                }
                else
                {
                    result = CreateProject(std::string(s_newName),
                                           std::filesystem::path(s_newFolder) / s_newName);
                    if (result == ProjectResult::Success)
                    {
                        // Clear fields on success.
                        s_newName[0] = '\0';
                        s_newFolder[0] = '\0';
                        s_newDesc[0] = '\0';
                        s_mode = Mode::Open; // flip back to open tab
                        s_selectedIndex = -1;
                    }
                }
                SetStatus(ResultString(result));
            }

            if (!canAct)
                ImGui::EndDisabled();

            // -- Status bar (below buttons) --
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
        UIRegistry::Get().Register([this]
                                   { DrawProjectManagerWindow(); });
        return true;
    }

    void ProjectManager::Update()
    {
    }

} // namespace SF::Engine