#include "ShaderIncludes.hpp"
#include <functional>
#include <iostream>
#include <regex>

namespace SF::Engine::Shaders
{
    ShaderIncludeResolver::ShaderIncludeResolver()
    {
        // Add default include directories
        includeDirs_.push_back(".");
        includeDirs_.push_back("./Shaders");
        // includeDirs_.push_back("./Shaders/Include");
    }

    void ShaderIncludeResolver::addIncludeDirectory(const std::string &path)
    {
        std::string normalized = ShaderIncludeUtils::normalizePath(path);

        // Check if directory exists
        if (!std::filesystem::exists(normalized))
        {
            std::cerr << "Warning: Include directory does not exist: " << normalized << std::endl;
            return;
        }

        // Don't add duplicates
        if (std::find(includeDirs_.begin(), includeDirs_.end(), normalized) == includeDirs_.end())
        {
            includeDirs_.push_back(normalized);
        }
    }

    void ShaderIncludeResolver::clearIncludeDirectories()
    {
        includeDirs_.clear();
    }

    void ShaderIncludeResolver::clearCache()
    {
        includeCache_.clear();
        importedFiles_.clear();
    }

    std::string ShaderIncludeResolver::findIncludeFile(const std::string &filename,
                                                       const std::string &basePath,
                                                       bool angleBracket) const
    {
        // Extensions to try when the filename has none.
        static const std::vector<std::string> kExts = {"", ".si", ".glsl", ".hlsl", ".h"};

        auto tryPath = [](const std::string &p) -> std::string
        {
            if (std::filesystem::exists(p))
                return std::filesystem::canonical(p).string();
            return {};
        };

        auto tryWithExts = [&](const std::string &base) -> std::string
        {
            for (const auto &ext : kExts)
            {
                auto r = tryPath(base + ext);
                if (!r.empty())
                    return r;
            }
            return {};
        };

        // 1. Relative to basePath — ONLY for quoted includes ("...").
        //    Angle-bracket includes (<...>) skip this step entirely and go
        //    straight to the registered include directories, just like C/C++.
        if (!angleBracket && !basePath.empty())
        {
            auto r = tryWithExts(ShaderIncludeUtils::combinePaths(basePath, filename));
            if (!r.empty())
                return r;
        }

        // 2. Registered include directories — always searched for both syntaxes.
        for (const auto &dir : includeDirs_)
        {
            auto r = tryWithExts(ShaderIncludeUtils::combinePaths(dir, filename));
            if (!r.empty())
                return r;
        }

        // 3. Absolute path fallback.
        {
            auto r = tryWithExts(filename);
            if (!r.empty())
                return r;
        }

        return "";
    }

    std::optional<std::string> ShaderIncludeResolver::resolveIncludes(const std::string &source,
                                                                      const std::string &basePath,
                                                                      bool useImportSemantics)
    {
        std::set<std::string> processedFiles;

        // Clear imported files if not using import semantics
        if (!useImportSemantics)
        {
            importedFiles_.clear();
        }

        return resolveIncludesRecursive(source, basePath, processedFiles, 0);
    }

    std::optional<std::string> ShaderIncludeResolver::resolveIncludesRecursive(
        const std::string &source, const std::string &currentPath,
        std::set<std::string> &processedFiles, int depth)
    {
        if (depth > MAX_INCLUDE_DEPTH)
        {
            setError("Maximum include depth exceeded (" + std::to_string(MAX_INCLUDE_DEPTH) + ")");
            return std::nullopt;
        }

        std::stringstream result;
        std::istringstream sourceStream(source);
        std::string line;
        int lineNumber = 0;

        while (std::getline(sourceStream, line))
        {
            lineNumber++;
            bool isImport = false;
            bool isAngleBracket = false;
            std::string filename;

            if (ShaderIncludeUtils::isIncludeDirective(line, isImport, filename, isAngleBracket))
            {
                // "..." resolves relative to currentPath first, then include dirs.
                // <...> skips the relative step and only searches include dirs.
                std::string includeFilePath = findIncludeFile(filename, currentPath, isAngleBracket);

                if (includeFilePath.empty())
                {
                    setError("Failed to find include file: " + filename +
                             " (referenced at line " + std::to_string(lineNumber) + ")");
                    return std::nullopt;
                }

                // Normalize path for consistent comparisons.
                includeFilePath = ShaderIncludeUtils::normalizePath(includeFilePath);

                // Guard: skip circular includes.
                if (processedFiles.find(includeFilePath) != processedFiles.end())
                {
                    if (trackDepth_)
                        result << "// [Circular include skipped: " << filename << "]\n";
                    continue;
                }

                // Guard: skip already-imported files (#import once-per-file semantics).
                if (isImport && importedFiles_.find(includeFilePath) != importedFiles_.end())
                {
                    if (trackDepth_)
                        result << "// [Already imported: " << filename << "]\n";
                    continue;
                }

                // Load (from cache or disk).
                auto includeOpt = loadInclude(includeFilePath);
                if (!includeOpt)
                    return std::nullopt;

                ShaderInclude &include = *includeOpt;

                // Mark in-flight to catch circular deps in recursive calls.
                processedFiles.insert(includeFilePath);
                if (isImport)
                    importedFiles_.insert(includeFilePath);

                if (trackDepth_)
                    result << "// [Begin include: " << filename << " (depth: " << depth << ")]\n";

                // Nested includes are always resolved relative to the included file's
                // own directory, regardless of whether it was angle-bracket or quoted.
                std::string includeDir = ShaderIncludeUtils::getDirectory(includeFilePath);

                auto processedInclude = resolveIncludesRecursive(
                    include.content, includeDir, processedFiles, depth + 1);

                if (!processedInclude)
                    return std::nullopt;

                result << *processedInclude;

                if (trackDepth_)
                    result << "// [End include: " << filename << "]\n";

                // Remove the in-flight guard so the same file can be legitimately
                // included again from a different branch (only #import prevents that).
                processedFiles.erase(includeFilePath);
            }
            else
            {
                result << line << "\n";
            }
        }

        return result.str();
    }

    std::vector<ShaderIncludeResolver::IncludeDirective>
    ShaderIncludeResolver::parseIncludeDirectives(const std::string &source)
    {
        std::vector<IncludeDirective> directives;
        std::istringstream stream(source);
        std::string line;
        size_t offset = 0;

        while (std::getline(stream, line))
        {
            bool isImport;
            bool isAngleBracket;
            std::string filename;
            if (ShaderIncludeUtils::isIncludeDirective(line, isImport, filename, isAngleBracket))
            {
                IncludeDirective dir;
                dir.startPos = offset;
                dir.endPos = offset + line.length();
                dir.filename = filename;
                dir.isImport = isImport;
                directives.push_back(dir);
            }
            offset += line.length() + 1; // +1 for newline
        }

        return directives;
    }

    std::optional<ShaderInclude> ShaderIncludeResolver::loadInclude(const std::string &filepath)
    {
        // Check cache first
        auto it = includeCache_.find(filepath);
        if (it != includeCache_.end())
        {
            return it->second;
        }

        // Load from file
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            setError("Failed to open include file: " + filepath);
            return std::nullopt;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        ShaderInclude include;
        include.path = filepath;
        include.content = buffer.str();
        include.processed = false;

        // Cache it
        includeCache_[filepath] = include;

        return include;
    }

    std::vector<std::string> ShaderIncludeResolver::getDependencies(const std::string &filepath)
    {
        std::vector<std::string> dependencies;
        std::set<std::string> visited;

        std::function<void(const std::string &)> collectDeps = [&](const std::string &file)
        {
            if (visited.find(file) != visited.end())
                return;

            visited.insert(file);

            auto includeOpt = loadInclude(file);
            if (!includeOpt)
                return;

            // Parse for include directives
            std::istringstream stream(includeOpt->content);
            std::string line;

            while (std::getline(stream, line))
            {
                bool isImport;
                bool isAngleBracket;
                std::string filename;
                if (ShaderIncludeUtils::isIncludeDirective(line, isImport, filename, isAngleBracket))
                {
                    std::string includeDir = ShaderIncludeUtils::getDirectory(file);
                    std::string includePath = findIncludeFile(filename, includeDir);

                    if (!includePath.empty())
                    {
                        dependencies.push_back(includePath);
                        collectDeps(includePath);
                    }
                }
            }
        };

        collectDeps(filepath);
        return dependencies;
    }

    std::string ShaderIncludeResolver::resolveIncludePath(const std::string &filename,
                                                          const std::string &basePath) const
    {
        return findIncludeFile(filename, basePath);
    }

    bool ShaderIncludeResolver::hasCircularDependency(
        const std::string &filepath, const std::set<std::string> &processedFiles) const
    {
        return processedFiles.find(filepath) != processedFiles.end();
    }

    void ShaderIncludeResolver::setError(const std::string &error)
    {
        lastError_ = error;
        std::cerr << "ShaderIncludeResolver Error: " << error << std::endl;
    }

} // namespace SF::Engine::Shaders