#pragma once
#include <Engine/Module.hpp>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace SF::Engine
{
    // Forward declarations
    class XMLNode;
    class Serializable;

    // XML Attribute
    struct XMLAttribute
    {
        std::string name;
        std::string value;
    };

    // XML Node wrapper for libxml2
    class XMLNode
    {
    public:
        XMLNode() : node(nullptr), doc(nullptr) {}
        explicit XMLNode(xmlNodePtr ptr) : node(ptr), doc(nullptr) {}
        XMLNode(xmlNodePtr ptr, xmlDocPtr document) : node(ptr), doc(document) {}

        ~XMLNode() = default;

        // Node properties
        std::string GetName() const;
        std::string GetContent() const;
        void SetContent(const std::string &content);

        // Attribute handling
        std::string GetAttribute(const std::string &name) const;
        bool GetAttribute(const std::string &name, std::string &out) const;
        bool GetAttribute(const std::string &name, int &out) const;
        bool GetAttribute(const std::string &name, float &out) const;
        bool GetAttribute(const std::string &name, bool &out) const;

        void SetAttribute(const std::string &name, const std::string &value);
        void SetAttribute(const std::string &name, int value);
        void SetAttribute(const std::string &name, float value);
        void SetAttribute(const std::string &name, bool value);

        // Child node operations
        XMLNode GetFirstChild() const;
        XMLNode GetNextSibling() const;
        XMLNode GetChild(const std::string &name) const;
        std::vector<XMLNode> GetChildren(const std::string &name = "") const;

        XMLNode AddChild(const std::string &name);
        XMLNode AddChild(const std::string &name, const std::string &content);

        // Validation
        bool IsValid() const { return node != nullptr; }

    private:
        xmlNodePtr node;
        xmlDocPtr doc;
        friend class XMLReader;
    };

    // Serializable interface
    class Serializable
    {
    public:
        virtual ~Serializable() = default;
        virtual void Serialize(XMLNode &node) const = 0;
        virtual void Deserialize(const XMLNode &node) = 0;
    };

    // Main XML Reader/Writer class
    class XMLReader : public ModuleRegistrar<XMLReader> // Why the fuck was this SceneManager?
    {
        REGISTER_MODULE(XMLReader, ModuleStage::Normal, Requires<>{});

    public:
        XMLReader();
        ~XMLReader();

        // Prevent copying
        XMLReader(const XMLReader &) = delete;
        XMLReader &operator=(const XMLReader &) = delete;

        void Update() override {}

        // File operations
        bool LoadFromFile(const std::string &filename);
        bool SaveToFile(const std::string &filename) const;

        bool LoadFromString(const std::string &content);
        std::string SaveToString() const;

        // Root node access
        XMLNode GetRootNode() const;
        void SetRootNode(const std::string &rootName);

        // Serialization helpers
        template <typename T>
        bool Serialize(const std::string &name, const T &object, const std::string &filename);

        template <typename T>
        bool Deserialize(const std::string &filename, T &object);

        // Type-specific serialization
        static std::string SerializeValue(int value);
        static std::string SerializeValue(float value);
        static std::string SerializeValue(bool value);
        static std::string SerializeValue(const std::string &value);

        static bool DeserializeValue(const std::string &str, int &out);
        static bool DeserializeValue(const std::string &str, float &out);
        static bool DeserializeValue(const std::string &str, bool &out);
        static bool DeserializeValue(const std::string &str, std::string &out);

        // Error handling
        std::string GetLastError() const { return lastError; }

    private:
        xmlDocPtr document;
        xmlNodePtr rootNode;
        std::string lastError;

        void Clear();
        void SetError(const std::string &error);
        static void ErrorHandler(void *ctx, const char *msg, ...);
    };
}

// Include implementation
#include "XMLReader.inl"