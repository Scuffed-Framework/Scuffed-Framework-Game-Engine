#pragma once
#include <string>
#include <vector>
#include <Filesystem/File.hpp>
#include <XML/XMLModule.hpp>
#include <Engine/Module.hpp>
#include <Gui/UIRegistry.hpp>
#include <Filesystem/ImGuiFileDialog.hpp>
#include <Rendering/Images/Image2d.hpp>

namespace SF::Engine
{
    struct ProjectTemplate
    {
        std::shared_ptr<Image2d> ExampleImage;
        std::string name;
        std::string description;
    };

    struct ProjectLoadeInfo
    {
        std::shared_ptr<Image2d> aFrame;
        std::string name;
        std::filesystem::path projectPath;
        std::string description;
    };
    struct ProjectCreateInfo
    {
        std::string &name; // not null
        std::filesystem::path projectPath;
        std::string description;
    };

    enum ProjectResult
    {
        Success,
        NotFound,
        InvalidFormat,
        VersionMismatch,
        UnknownError
    };

    class Project : public Serializable
    {
    public:
        std::string name;
        std::filesystem::path Path;
        File projectXML;

        void Serialize(XMLNode &node) const override
        {
            node.SetAttribute("name", name);
            node.SetAttribute("projectFilePath", Path.string());
        }
        void Deserialize(const XMLNode &node)
        {
            node.GetAttribute("name", name);
            std::string pathStr;
            node.GetAttribute("projectFilePath", pathStr);
            Path = pathStr;
        }
    };

    class ProjectManager : public ModuleRegistrar<ProjectManager>
    {
        friend class ModuleRegistrar<ProjectManager>;
        REGISTER_MODULE(ProjectManager, Module::Stage::Normal);

    public:
        ProjectResult CreateProject(const std::string &name, const std::filesystem::path &path);
        ProjectResult LoadProject(const std::filesystem::path &path);

        void Update() override;
        bool Initialize() override;
        void DrawProjectManagerWindow();

        bool IsAProjectLoaded() const
        {
            return currentLoadedProject != nullptr;
        }

        std::filesystem::path GetProjectPath() { return currentLoadedProject->Path; }
        std::filesystem::path GetProjectAssetPath() { return currentLoadedProject->Path / "Assets"; }
        std::filesystem::path GetProjectLogPath() { return currentLoadedProject->Path / "Logs"; }
        std::filesystem::path GetProjectCachePath() { return currentLoadedProject->Path / "Cache"; }
        std::filesystem::path GetProjectBuildPath() { return currentLoadedProject->Path / "Build"; }

        Project *GetCurrentProject() { return currentLoadedProject; }

    private:
        Project *currentLoadedProject = nullptr;
        bool projectWindowOpen = true; // true by default
    };
    namespace
    {
        enum class Mode
        {
            Open,
            Create
        };

        static Mode s_mode = Mode::Open;
        static int s_selectedIndex = -1;

        static char s_newName[256] = "";
        static char s_newFolder[512] = "";
        static char s_newDesc[1024] = "";

        static std::vector<ProjectLoadeInfo> s_recentProjects;
        static bool s_recentLoaded = false;

        static std::vector<ProjectTemplate> s_templates;
        static bool s_templatesLoaded = false;

        static std::string s_statusMsg;
        static float s_statusTimer = 0.0f;
        constexpr float kStatusDuration = 3.0f; // seconds

        void SetStatus(const char *msg)
        {
            s_statusMsg = msg;
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
        void DrawPreviewImage(const Image2d *img, ImVec2 size)
        {
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImDrawList *dl = ImGui::GetWindowDrawList();

            if (img)
            {
                // Wire up texture handle here:
                // ImGui::Image((ImTextureID)(intptr_t)img->GetTextureID(), size);
                // For now draw a tinted placeholder so it looks distinct.
                dl->AddRectFilled(cursor, {cursor.x + size.x, cursor.y + size.y},
                                  IM_COL32(30, 50, 70, 255));
                dl->AddRect(cursor, {cursor.x + size.x, cursor.y + size.y},
                            IM_COL32(80, 140, 200, 255));
                const char *lbl = "Preview";
                ImVec2 lsize = ImGui::CalcTextSize(lbl);
                dl->AddText({cursor.x + (size.x - lsize.x) * 0.5f,
                             cursor.y + (size.y - lsize.y) * 0.5f},
                            IM_COL32(80, 140, 200, 255), lbl);
            }
            else
            {
                dl->AddRectFilled(cursor, {cursor.x + size.x, cursor.y + size.y},
                                  IM_COL32(45, 45, 45, 255));
                dl->AddRect(cursor, {cursor.x + size.x, cursor.y + size.y},
                            IM_COL32(100, 100, 100, 255));
                const char *lbl = "No Preview";
                ImVec2 lsize = ImGui::CalcTextSize(lbl);
                dl->AddText({cursor.x + (size.x - lsize.x) * 0.5f,
                             cursor.y + (size.y - lsize.y) * 0.5f},
                            IM_COL32(130, 130, 130, 255), lbl);
            }
            ImGui::Dummy(size);
        }

        // Returns true on double-click (caller should treat as "confirm").
        bool SelectableItem(const std::string &label, bool selected, int index)
        {
            if (ImGui::Selectable(label.c_str(), selected,
                                  ImGuiSelectableFlags_AllowDoubleClick))
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
    }
}