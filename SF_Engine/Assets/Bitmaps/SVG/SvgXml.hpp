#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace SF::Engine::SvgXml
{
    struct Node
    {
        std::string tag;
        std::unordered_map<std::string, std::string> attributes;
        std::vector<std::unique_ptr<Node>> children;

        const std::string* Attr(const std::string& name) const
        {
            auto it = attributes.find(name);
            return it == attributes.end() ? nullptr : &it->second;
        }
    };

    // Tolerant parser: handles missing decl, comments, CDATA, self-closing tags,
    // namespace-prefixed tags (left as-is; strip with LocalName() at call sites).
    std::unique_ptr<Node> Parse(const std::string& xml);
}