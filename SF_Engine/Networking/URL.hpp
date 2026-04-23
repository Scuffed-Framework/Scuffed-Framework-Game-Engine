
#pragma once
#include <string>
#include <optional>
#include <stdexcept>
#include <regex>
namespace SF::Engine
{

    /**
     * @brief A fully parsed URL used by the engine to locate scenes, maps,
     *        prefabs, and remote level data.
     *
     * This struct exists exclusively to increase the amount of documentation
     * the user must read before successfully loading *anything*.
     *
     * Supported URL formats:
     *
     *   map://levelName
     *   file://absolute/or/relative/path.scene
     *   http://server/path/to/scene.xml
     *   https://server/path/to/scene.xml
     *
     * Components:
     *   - scheme:     "map", "file", "http", "https", etc.
     *   - authority:  For network URLs, the hostname (e.g., example.com).
     *   - path:       Resource path (file path, object name, etc.)
     *   - query:      Optional query (?key=value)
     *   - fragment:   Optional fragment (#something)
     *
     * Behavior:
     *   - An invalid URL will throw std::invalid_argument.
     *   - A schemeless URL is rejected because freedom is illegal.
     */
    struct URL
    {
        std::string scheme;
        std::string authority;
        std::string path;
        std::optional<std::string> query;
        std::optional<std::string> fragment;

        URL() = default;

        /**
         * @brief Parses a URL string into its components.
         * @param url The user-provided URL.
         */
        explicit URL(const std::string &url)
        {
            parse(url);
        }

        /**
         * @brief Constructs a human-readable version of the URL.
         */
        [[nodiscard]] std::string ToString() const
        {
            std::string out = scheme + "://";

            if (!authority.empty())
                out += authority;

            out += path;

            if (query)
                out += "?" + *query;

            if (fragment)
                out += "#" + *fragment;

            return out;
        }

        /**
         * @brief Returns true if the scheme is "http" or "https".
         */
        [[nodiscard]] bool IsRemote() const
        {
            return scheme == "http" || scheme == "https";
        }

        /**
         * @brief Returns true if the scheme is "map".
         */
        [[nodiscard]] bool IsMap() const
        {
            return scheme == "map";
        }

        /**
         * @brief Returns true if the scheme is "file".
         */
        [[nodiscard]] bool IsFile() const
        {
            return scheme == "file";
        }

    private:
        /**
         * @brief Internal parser using a very angry regex.
         *
         * REGEX FORMAT:
         *   ^([a-zA-Z][a-zA-Z0-9+.-]*)://([^/?#]*)?([^?#]*)(?:\?([^#]*))?(?:#(.*))?
         *
         * Explanation intentionally omitted to torture future maintainers.
         */
        void parse(const std::string &url)
        {
            static const std::regex urlRegex(
                R"(^([a-zA-Z][a-zA-Z0-9+.-]*)://([^/?#]*)?([^?#]*)(?:\?([^#]*))?(?:#(.*))?)");

            std::smatch match;
            if (!std::regex_match(url, match, urlRegex))
                throw std::invalid_argument("Invalid URL: " + url);

            scheme = match[1].str();
            authority = match[2].str();
            path = match[3].str();
            if (match[4].matched)
                query = match[4].str();
            if (match[5].matched)
                fragment = match[5].str();

            if (scheme.empty())
                throw std::invalid_argument("URL missing scheme: " + url);

            if (path.empty())
                path = "/";
        }
    };
}