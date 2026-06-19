#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <algorithm>

namespace SF::Engine::Shaders
{
    /**
     * @brief Represents a single shader include file
     */
    struct ShaderInclude
    {
        std::string path;                      // Full resolved path
        std::string content;                   // File content
        std::vector<std::string> dependencies; // Paths of files this includes
        bool processed = false;                // Whether dependencies have been resolved
    };

    /**
     * @brief Manages shader include files with caching and dependency resolution
     */
    class ShaderIncludeResolver
    {
    public:
        ShaderIncludeResolver();
        ~ShaderIncludeResolver() = default;

        /**
         * @brief Add a directory to search for include files
         * @param path Directory path to add
         */
        void addIncludeDirectory(const std::string &path);

        /**
         * @brief Clear all include directories
         */
        void clearIncludeDirectories();

        /**
         * @brief Get all include directories
         */
        const std::vector<std::string> &getIncludeDirectories() const { return includeDirs_; }

        /**
         * @brief Resolve and process all #include and #import directives in source
         * @param source Shader source code
         * @param basePath Base path for relative includes (usually the shader file's directory)
         * @param useImportSemantics If true, includes are only processed once (like #pragma once)
         * @return Processed source with includes expanded, or nullopt on error
         */
        std::optional<std::string> resolveIncludes(const std::string &source,
                                                   const std::string &basePath = "",
                                                   bool useImportSemantics = false);

        /**
         * @brief Clear the include cache
         */
        void clearCache();

        /**
         * @brief Get the last error message
         */
        const std::string &getLastError() const { return lastError_; }

        /**
         * @brief Check if a file exists in include paths
         * @param filename Filename to search for
         * @param basePath Base path for relative search
         * @return Full path if found, empty string otherwise
         */
        std::string findIncludeFile(const std::string &filename, const std::string &basePath = "", bool angleBracket = false) const;

        /**
         * @brief Get all dependencies for a shader file
         * @param filepath Shader file path
         * @return Vector of all included file paths
         */
        std::vector<std::string> getDependencies(const std::string &filepath);

        /**
         * @brief Enable/disable include depth tracking (for debugging)
         */
        void setTrackIncludeDepth(bool enable) { trackDepth_ = enable; }

    private:
        /**
         * @brief Internal recursive include resolution
         */
        std::optional<std::string> resolveIncludesRecursive(
            const std::string &source,
            const std::string &currentPath,
            std::set<std::string> &processedFiles,
            int depth);

        /**
         * @brief Load a shader include file
         */
        std::optional<ShaderInclude> loadInclude(const std::string &filepath);

        /**
         * @brief Parse include directives from source
         */
        struct IncludeDirective
        {
            size_t startPos;
            size_t endPos;
            std::string filename;
            bool isImport; // true for #import, false for #include
        };
        std::vector<IncludeDirective> parseIncludeDirectives(const std::string &source);

        /**
         * @brief Resolve a filename to full path
         */
        std::string resolveIncludePath(const std::string &filename, const std::string &basePath) const;

        /**
         * @brief Check for circular dependencies
         */
        bool hasCircularDependency(const std::string &filepath,
                                   const std::set<std::string> &processedFiles) const;

        void setError(const std::string &error);

        std::vector<std::string> includeDirs_;
        std::map<std::string, ShaderInclude> includeCache_;
        std::set<std::string> importedFiles_; // For #import semantics
        std::string lastError_;
        bool trackDepth_ = false;
        static constexpr int MAX_INCLUDE_DEPTH = 32;
    };

    /**
     * @brief Utility class for shader include file operations
     */
    class ShaderIncludeUtils
    {
    public:
        /**
         * @brief Strip comments from shader source
         */
        static std::string stripComments(const std::string &source);

        /**
         * @brief Check if a line contains an include directive
         */
        static bool isIncludeDirective(const std::string &line, bool &isImport, std::string &filename, bool &isAngleBracket);

        /**
         * @brief Extract filename from include directive
         * Supports: #include "file.h", #include <file.h>, #import "file.si"
         */
        static std::optional<std::string> extractIncludeFilename(const std::string &line, bool &isAngleBracket);

        /**
         * @brief Normalize path separators (convert to forward slashes)
         */
        static std::string normalizePath(const std::string &path);

        /**
         * @brief Get the directory portion of a file path
         */
        static std::string getDirectory(const std::string &filepath);

        /**
         * @brief Check if a path is absolute
         */
        static bool isAbsolutePath(const std::string &path);

        /**
         * @brief Combine two paths
         */
        static std::string combinePaths(const std::string &base, const std::string &relative);

        static std::string stripSectionDirectives(const std::string &source)
        {
            std::string result;
            std::istringstream stream(source);
            std::string line;
            while (std::getline(stream, line))
            {
                // Trim leading whitespace
                size_t first = line.find_first_not_of(" \t");
                if (first != std::string::npos && line.substr(first, 8) == "#Section")
                    continue; // skip this line
                result += line + "\n";
            }
            return result;
        }
    };

    // Implementation of utility functions
    inline std::string ShaderIncludeUtils::stripComments(const std::string &source)
    {
        std::string result;
        result.reserve(source.length());

        bool inLineComment = false;
        bool inBlockComment = false;
        bool inString = false;
        bool inSection = false;

        for (size_t i = 0; i < source.length(); i++)
        {
            char c = source[i];
            char next = (i + 1 < source.length()) ? source[i + 1] : '\0';

            if (inString)
            {
                result += c;
                if (c == '"' && (i == 0 || source[i - 1] != '\\'))
                    inString = false;
                continue;
            }

            if (inLineComment)
            {
                if (c == '\n')
                {
                    inLineComment = false;
                    result += c;
                }
                continue;
            }

            if (inBlockComment)
            {
                if (c == '*' && next == '/')
                {
                    inBlockComment = false;
                    i++;
                }
                else if (c == '\n')
                {
                    result += c;
                }
                continue;
            }

            if (inSection)
            {
                if (c == '\n')
                    inSection = false;
                result += c;
                continue;
            }

            if (c == '#' && next == 'S' && !inSection)
            {
                inSection = true;
                result += c;
                continue;
            }

            if (c == '/' && next == '/')
            {
                inLineComment = true;
                i++;
                continue;
            }

            if (c == '/' && next == '*')
            {
                inBlockComment = true;
                i++;
                continue;
            }

            if (c == '"')
                inString = true;

            result += c;
        }

        return result;
    }

    inline bool ShaderIncludeUtils::isIncludeDirective(const std::string &line, bool &isImport,
                                                       std::string &filename, bool &isAngleBracket)
    {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] != '#')
            return false;

        size_t pos = start + 1;
        while (pos < line.length() && std::isspace(line[pos]))
            pos++;

        if (line.substr(pos, 7) == "include")
        {
            isImport = false;
            pos += 7;
        }
        else if (line.substr(pos, 6) == "import")
        {
            isImport = true;
            pos += 6;
        }
        else
        {
            return false;
        }

        auto filenameOpt = extractIncludeFilename(line.substr(pos), isAngleBracket);
        if (!filenameOpt)
            return false;

        filename = *filenameOpt;
        return true;
    }

    inline std::optional<std::string> ShaderIncludeUtils::extractIncludeFilename(
        const std::string &line, bool &isAngleBracket)
    {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
            return std::nullopt;

        char delimiter = line[start];
        if (delimiter != '"' && delimiter != '<')
            return std::nullopt;

        isAngleBracket = (delimiter == '<');
        char endDelimiter = isAngleBracket ? '>' : '"';

        size_t end = line.find(endDelimiter, start + 1);
        if (end == std::string::npos)
            return std::nullopt;

        return line.substr(start + 1, end - start - 1);
    }

    inline std::string ShaderIncludeUtils::normalizePath(const std::string &path)
    {
        std::string result = path;
        std::replace(result.begin(), result.end(), '\\', '/');
        return result;
    }

    inline std::string ShaderIncludeUtils::getDirectory(const std::string &filepath)
    {
        std::filesystem::path p(filepath);
        return p.parent_path().string();
    }

    inline bool ShaderIncludeUtils::isAbsolutePath(const std::string &path)
    {
        std::filesystem::path p(path);
        return p.is_absolute();
    }

    inline std::string ShaderIncludeUtils::combinePaths(const std::string &base, const std::string &relative)
    {
        if (base.empty())
            return relative;
        if (relative.empty())
            return base;

        std::filesystem::path basePath(base);
        std::filesystem::path relativePath(relative);

        if (relativePath.is_absolute())
            return relative;

        return (basePath / relativePath).string();
    }

} // namespace SF::Engine::Shaders