#pragma once
#include <filesystem>
#include <vector>

namespace SF::Engine
{
    void CreateShaderWithStages(const std::vector<std::string> &stages, const std::filesystem::path &path,
                                const std::string &name);
    void ShowCreateShaderWizard(std::filesystem::path path);
    void CreateShaderInclude(std::filesystem::path path, const std::string &incGaurdName);
    void ShowCreateShaderIncludeWizard(std::filesystem::path path);
} // namespace SF::Engine
