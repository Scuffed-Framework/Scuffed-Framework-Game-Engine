#pragma once
#include <Engine/Module.hpp>
#include <Gui/ImGui/FileDialog/ImGuiFileDialog.hpp>
#include <Gui/ImGui/UIRegistry.hpp>
#include <LowLevel/FileSystem/File.hpp>
#include <LowLevel/XML/XMLModule.hpp>
#include <Rendering/RHI/Images/Image.hpp>
#include <string>
#include <vector>

#ifdef Success
    #undef Success
#endif

namespace SF::Engine
{
    constexpr string_view ProjectFileExtension = ".projxml";
    struct ProjectTemplate
    {
        std::shared_ptr<Image2d> ExampleImage;
        std::string name;
        std::string description;
    };

    struct ProjectLoadInfo
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
        std::string description;

        void Serialize(XMLNode &node) const override
        {
            XMLNode projNode = node.AddChild("Project");
            projNode.SetAttribute("name", name);
            projNode.SetAttribute("projectFilePath", Path.string());
            projNode.SetAttribute("Description", description);
        }
        void Deserialize(const XMLNode &node) override
        {
            XMLNode projNode = node.GetChild("Project");
            projNode.GetAttribute("name", name);
            std::string pathStr;
            projNode.GetAttribute("projectFilePath", pathStr);
            projNode.GetAttribute("Description", description);
            Path = pathStr;
        }
    };

    class ProjectManager : public ModuleRegistrar<ProjectManager>
    {
        friend class ModuleRegistrar<ProjectManager>;
        REGISTER_MODULE(ProjectManager, Module::Stage::Normal);

    public:
        ProjectResult CreateProject(const std::string &name, const std::filesystem::path &path,
                                    const std::string &desc);
        ProjectResult LoadProject(const std::filesystem::path &path);

        [[nodiscard]] ProjectResult SaveProject() const;

        void Update() override;
        bool Initialize() override;
        void DrawProjectManagerWindow();
        void OpenProjectManagerWindow() { projectWindowOpen = true; }

        [[nodiscard]] bool IsAProjectLoaded() const { return currentLoadedProject != nullptr; }

        [[nodiscard]] std::filesystem::path GetProjectPath() const { return currentLoadedProject->Path.parent_path(); }
        [[nodiscard]] std::filesystem::path GetProjectAssetPath() const
        {
            return currentLoadedProject->Path.parent_path() / "Assets";
        }
        [[nodiscard]] std::filesystem::path GetProjectLogPath() const
        {
            return currentLoadedProject->Path.parent_path() / "Logs";
        }
        [[nodiscard]] std::filesystem::path GetProjectCachePath() const
        {
            return currentLoadedProject->Path.parent_path() / "Cache";
        }
        [[nodiscard]] std::filesystem::path GetProjectBuildPath() const
        {
            return currentLoadedProject->Path.parent_path() / "Build";
        }

        [[nodiscard]] Project *GetCurrentProject() const { return currentLoadedProject.get(); }

    private:
        std::unique_ptr<Project> currentLoadedProject;
        bool projectWindowOpen = true; // true by default
    };


} // namespace SF::Engine
