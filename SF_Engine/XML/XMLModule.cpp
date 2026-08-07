#include "XMLModule.hpp"
#include <Engine/Log/Log.hpp>
#include <sstream>
#include <cstdarg>
#include <cstdio>

namespace SF::Engine
{

    // XMLNode

    std::string XMLNode::GetName() const
    {
        if (!node || !node->name)
            return {};
        return reinterpret_cast<const char *>(node->name);
    }

    std::string XMLNode::GetContent() const
    {
        if (!node)
            return {};
        xmlChar *content = xmlNodeGetContent(node);
        if (!content)
            return {};
        std::string result(reinterpret_cast<const char *>(content));
        xmlFree(content);
        return result;
    }

    void XMLNode::SetContent(const std::string &content)
    {
        if (!node)
            return;
        xmlNodeSetContent(node, reinterpret_cast<const xmlChar *>(content.c_str()));
    }

    // Attribute getters

    std::string XMLNode::GetAttribute(const std::string &name) const
    {
        if (!node)
            return {};
        xmlChar *val = xmlGetProp(node, reinterpret_cast<const xmlChar *>(name.c_str()));
        if (!val)
            return {};
        std::string result(reinterpret_cast<const char *>(val));
        xmlFree(val);
        return result;
    }

    bool XMLNode::GetAttribute(const std::string &name, std::string &out) const
    {
        if (!node)
            return false;
        xmlChar *val = xmlGetProp(node, reinterpret_cast<const xmlChar *>(name.c_str()));
        if (!val)
            return false;
        out = reinterpret_cast<const char *>(val);
        xmlFree(val);
        return true;
    }

    bool XMLNode::GetAttribute(const std::string &name, int &out) const
    {
        std::string s;
        if (!GetAttribute(name, s))
            return false;
        try
        {
            out = std::stoi(s);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool XMLNode::GetAttribute(const std::string &name, float &out) const
    {
        std::string s;
        if (!GetAttribute(name, s))
            return false;
        try
        {
            out = std::stof(s);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool XMLNode::GetAttribute(const std::string &name, bool &out) const
    {
        std::string s;
        if (!GetAttribute(name, s))
            return false;
        out = (s == "true" || s == "1");
        return true;
    }

    // Attribute setters

    void XMLNode::SetAttribute(const std::string &name, const std::string &value)
    {
        if (!node)
            return;
        xmlSetProp(node,
                   reinterpret_cast<const xmlChar *>(name.c_str()),
                   reinterpret_cast<const xmlChar *>(value.c_str()));
    }

    void XMLNode::SetAttribute(const std::string &name, int value)
    {
        SetAttribute(name, std::to_string(value));
    }

    void XMLNode::SetAttribute(const std::string &name, float value)
    {
        std::ostringstream ss;
        ss.precision(8);
        ss << value;
        SetAttribute(name, ss.str());
    }

    void XMLNode::SetAttribute(const std::string &name, bool value)
    {
        SetAttribute(name, value ? std::string("true") : std::string("false"));
    }

    // Child navigation

    XMLNode XMLNode::GetFirstChild() const
    {
        if (!node)
            return {};
        for (xmlNodePtr n = node->children; n; n = n->next)
            if (n->type == XML_ELEMENT_NODE)
                return XMLNode(n, doc);
        return {};
    }

    XMLNode XMLNode::GetNextSibling() const
    {
        if (!node)
            return {};
        for (xmlNodePtr n = node->next; n; n = n->next)
            if (n->type == XML_ELEMENT_NODE)
                return XMLNode(n, doc);
        return {};
    }

    XMLNode XMLNode::GetChild(const std::string &name) const
    {
        if (!node)
            return {};
        for (xmlNodePtr n = node->children; n; n = n->next)
        {
            if (n->type != XML_ELEMENT_NODE)
                continue;
            if (name.empty() ||
                std::string(reinterpret_cast<const char *>(n->name)) == name)
                return XMLNode(n, doc);
        }
        return {};
    }

    std::vector<XMLNode> XMLNode::GetChildren(const std::string &name) const
    {
        std::vector<XMLNode> result;
        if (!node)
            return result;
        for (xmlNodePtr n = node->children; n; n = n->next)
        {
            if (n->type != XML_ELEMENT_NODE)
                continue;
            if (name.empty() ||
                std::string(reinterpret_cast<const char *>(n->name)) == name)
                result.emplace_back(n, doc);
        }
        return result;
    }

    XMLNode XMLNode::AddChild(const std::string &name)
    {
        if (!node)
            return {};
        xmlNodePtr child = xmlNewChild(
            node, nullptr,
            reinterpret_cast<const xmlChar *>(name.c_str()),
            nullptr);
        return XMLNode(child, doc);
    }

    XMLNode XMLNode::AddChild(const std::string &name, const std::string &content)
    {
        if (!node)
            return {};
        xmlNodePtr child = xmlNewChild(
            node, nullptr,
            reinterpret_cast<const xmlChar *>(name.c_str()),
            reinterpret_cast<const xmlChar *>(content.c_str()));
        return XMLNode(child, doc);
    }

    // XMLReader

    XMLReader::XMLReader() : document(nullptr), rootNode(nullptr)
    {
        xmlInitParser();
        xmlSetGenericErrorFunc(this, &XMLReader::ErrorHandler);
    }

    XMLReader::~XMLReader()
    {
        Clear();
        xmlCleanupParser();
    }

    void XMLReader::Clear()
    {
        if (document)
        {
            xmlFreeDoc(document);
            document = nullptr;
        }
        rootNode = nullptr;
        lastError.clear();
    }

    void XMLReader::SetError(const std::string &error)
    {
        lastError = error;
        Log::Error("XMLReader: {}", error);
    }

    void XMLReader::ErrorHandler(void *ctx, const char *msg, ...)
    {
        auto *self = static_cast<XMLReader *>(ctx);
        char buf[512];
        va_list args;
        va_start(args, msg);
        std::vsnprintf(buf, sizeof(buf), msg, args);
        va_end(args);
        if (self)
            self->lastError += buf;
    }

    // File / string I/O

    bool XMLReader::LoadFromFile(const std::string &filename)
    {
        Clear();
        document = xmlReadFile(filename.c_str(), nullptr, XML_PARSE_NOBLANKS);
        if (!document)
        {
            SetError("Failed to load XML file: " + filename);
            return false;
        }
        rootNode = xmlDocGetRootElement(document);
        return true;
    }

    bool XMLReader::SaveToFile(const std::string &filename) const
    {
        if (!document)
            return false;
        return xmlSaveFormatFileEnc(filename.c_str(), document, "UTF-8", 1) >= 0;
    }

    bool XMLReader::LoadFromString(const std::string &content)
    {
        Clear();
        document = xmlReadMemory(
            content.c_str(), static_cast<int>(content.size()),
            "noname.xml", nullptr, XML_PARSE_NOBLANKS);
        if (!document)
        {
            SetError("Failed to parse XML string");
            return false;
        }
        rootNode = xmlDocGetRootElement(document);
        return true;
    }

    std::string XMLReader::SaveToString() const
    {
        if (!document)
            return {};
        xmlChar *buf = nullptr;
        int size = 0;
        xmlDocDumpFormatMemoryEnc(document, &buf, &size, "UTF-8", 1);
        if (!buf)
            return {};
        std::string result(reinterpret_cast<const char *>(buf), size);
        xmlFree(buf);
        return result;
    }

    // Root node

    XMLNode XMLReader::GetRootNode() const
    {
        return XMLNode(rootNode, document);
    }

    void XMLReader::SetRootNode(const std::string &rootName)
    {
        Clear();
        document = xmlNewDoc(reinterpret_cast<const xmlChar *>("1.0"));
        rootNode = xmlNewNode(nullptr,
                              reinterpret_cast<const xmlChar *>(rootName.c_str()));
        xmlDocSetRootElement(document, rootNode);
    }

    // Static value helpers

    std::string XMLReader::SerializeValue(int value)
    {
        return std::to_string(value);
    }

    std::string XMLReader::SerializeValue(float value)
    {
        std::ostringstream ss;
        ss.precision(8);
        ss << value;
        return ss.str();
    }

    std::string XMLReader::SerializeValue(bool value)
    {
        return value ? "true" : "false";
    }

    std::string XMLReader::SerializeValue(const std::string &value)
    {
        return value;
    }

    bool XMLReader::DeserializeValue(const std::string &str, int &out)
    {
        try
        {
            out = std::stoi(str);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool XMLReader::DeserializeValue(const std::string &str, float &out)
    {
        try
        {
            out = std::stof(str);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool XMLReader::DeserializeValue(const std::string &str, bool &out)
    {
        out = (str == "true" || str == "1");
        return true;
    }

    bool XMLReader::DeserializeValue(const std::string &str, std::string &out)
    {
        out = str;
        return true;
    }

    std::string XMLReader::SerializeValue(const GUID &value)
    {
        return value.ToString(); // or whatever GUID's canonical string accessor is
    }

    bool XMLReader::DeserializeValue(const std::string &str, GUID &out)
    {
        out = GUID::FromString(str);
        return true;
    }

} // namespace SF::Engine
