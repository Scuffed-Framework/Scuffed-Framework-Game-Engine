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

#include <UtilityClasses/UUID.hpp>
#include <LowLevel/Reflection/RTTI/RTTI.hpp>

namespace SF::Engine
{
    class XMLNode;
    class Serializable;

    struct XMLAttribute
    {
        std::string name;
        std::string value;
    };

    class XMLNode
    {
    public:
        XMLNode() : node(nullptr), doc(nullptr) {}
        explicit XMLNode(xmlNodePtr ptr) : node(ptr), doc(nullptr) {}
        XMLNode(xmlNodePtr ptr, xmlDocPtr document) : node(ptr), doc(document) {}

        ~XMLNode() = default;

        std::string GetName() const;
        std::string GetContent() const;
        void SetContent(const std::string &content);

        std::string GetAttribute(const std::string &name) const;
        bool GetAttribute(const std::string &name, std::string &out) const;
        bool GetAttribute(const std::string &name, int &out) const;
        bool GetAttribute(const std::string &name, float &out) const;
        bool GetAttribute(const std::string &name, bool &out) const;

        template <typename T>
        bool GetAttribute(const std::string &name, T &out) const;
        template <typename T>
        void SetAttribute(const std::string &name, const T &value);

        template <typename T>
        bool GetChildContent(const std::string &childName, T &out) const;

        template <typename T>
        void SetChildContent(const std::string &childName, const T &value);

        // Bulk get attributes into a map
        std::unordered_map<std::string, std::string>
        GetAttributes(const std::vector<std::string> &keys) const
        {
            std::unordered_map<std::string, std::string> result;
            for (const auto &key : keys)
            {
                std::string val;
                if (GetAttribute(key, val))
                    result[key] = val;
            }
            return result;
        }

        void SetAttributes(const std::unordered_map<std::string, std::string> &attrs)
        {
            for (const auto &[key, val] : attrs)
                SetAttribute(key, val);
        }

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
        friend class XMLModule;
    };

    class Serializable
    {
        SF_RTTI_BASE(Serializable)
    public:
        virtual ~Serializable() = default;
        virtual void Serialize(XMLNode &node) const = 0;
        virtual void Deserialize(const XMLNode &node) = 0;
    };

    class XMLModule : public ModuleRegistrar<XMLModule>
    {
        REGISTER_MODULE(XMLModule, ModuleStage::Normal, Requires<>{});

    public:
        XMLModule();
        ~XMLModule();

        XMLModule(const XMLModule &) = delete;
        XMLModule &operator=(const XMLModule &) = delete;

        void Update() override {}

        bool LoadFromFile(const std::string &filename);
        bool SaveToFile(const std::string &filename) const;

        bool LoadFromString(const std::string &content);
        std::string SaveToString() const;

        XMLNode GetRootNode() const;
        void SetRootNode(const std::string &rootName);

        template <typename T>
        bool Serialize(const std::string &name, const T &object, const std::string &filename);

        template <typename T>
        bool Deserialize(const std::string &filename, T &object);

        // Type-specific serialization
        static std::string SerializeValue(int value);
        static std::string SerializeValue(float value);
        static std::string SerializeValue(bool value);
        static std::string SerializeValue(const std::string &value);
        static std::string SerializeValue(const UUID &value);

        static bool DeserializeValue(const std::string &str, int &out);
        static bool DeserializeValue(const std::string &str, float &out);
        static bool DeserializeValue(const std::string &str, bool &out);
        static bool DeserializeValue(const std::string &str, std::string &out);
        static bool DeserializeValue(const std::string &str, UUID &out);

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
#include "XMLModule.inl"