#pragma once
#include <string>
#include <vector>
#include <Filesystem/File.hpp>
#include <XML/XMLReader.hpp>
#include <Engine/Module.hpp>
#include <Gui/UIRegistry.hpp>
#include <Filesystem/ImGuiFileDialog.hpp>
#include <Graphics/Images/Image2d.hpp>

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

        std::filesystem::path GetProjectPath() { return currentLoadedProject->Path; }
        std::filesystem::path GetProjectAssetPath() { return currentLoadedProject->Path / "Assets"; }
        std::filesystem::path GetProjectLogPath() { return currentLoadedProject->Path / "Logs"; }
        std::filesystem::path GetProjectCachePath() { return currentLoadedProject->Path / "Cache"; }
        std::filesystem::path GetProjectBuildPath() { return currentLoadedProject->Path / "Build"; }

    private:
        Project *currentLoadedProject = nullptr;
        bool projectWindowOpen = true; // true by default
    };
}