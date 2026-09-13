#pragma once
#include <filesystem>
#include <vector>

namespace SF::Engine
{
    void CreateShaderWithStages(std::vector<std::string> stages, std::filesystem::path path, std::string name);
    void ShowCreateShaderWizard(std::filesystem::path path);
    void CreateShaderInclude(std::filesystem::path path, std::string incGaurdName);
    void ShowCreateShaderIncludeWizard(std::filesystem::path path);
} // namespace SF::Engine
