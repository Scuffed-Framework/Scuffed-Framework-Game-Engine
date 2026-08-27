#pragma once
#include <Reflection/RTTI/RTTICast.hpp>
#include <LowLevel/XML/XMLModule.hpp>
#include <Engine/Log/Log.hpp>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <type_traits>

namespace SF::Engine
{
    class SerializableRegistry
    {
    public:
        using FactoryFn = std::function<std::unique_ptr<Serializable>()>;

        static SerializableRegistry &Instance()
        {
            static SerializableRegistry instance;
            return instance;
        }

        template <typename T>
        void Register()
        {
            static_assert(std::is_base_of_v<Serializable, T>, "T must derive from Serializable.");
            static_assert(SF::RTTI::HasRtti<T>, "T needs SF_RTTI / SF_RTTI_BASE before it can be registered.");

            const std::string typeName = T::RTTI_TypeName();
            if (factories_.contains(typeName))
            {
                Log::Error("SerializableRegistry: '{}' registered more than once; ignoring duplicate.", typeName);
                return;
            }
            factories_.emplace(typeName, []
                               { return std::unique_ptr<Serializable>(std::make_unique<T>()); });
        }

        std::unique_ptr<Serializable> Create(const std::string &typeName) const
        {
            auto it = factories_.find(typeName);
            return it != factories_.end() ? it->second() : nullptr;
        }

    private:
        std::unordered_map<std::string, FactoryFn> factories_;
    };

    template <typename T>
    struct SerializableRegistrar
    {
        SerializableRegistrar() { SerializableRegistry::Instance().template Register<T>(); }
    };

    namespace RttiXml
    {
        inline constexpr const char *kTypeAttribute = "TypeName";

        // Writes <elementName TypeName="ConcreteType"> + obj's own Serialize().
        inline void WritePolymorphic(XMLNode &parent, const std::string &elementName, const Serializable &obj)
        {
            XMLNode node = parent.AddChild(elementName);
            node.SetAttribute(kTypeAttribute, std::string(obj.RTTI_GetTypeName()));
            obj.Serialize(node);
        }

        inline std::unique_ptr<Serializable> ReadPolymorphic(const XMLNode &node)
        {
            std::string typeName;
            if (!node.GetAttribute(kTypeAttribute, typeName))
            {
                Log::Error("RttiXml::ReadPolymorphic: node '{}' has no '{}' attribute.",
                           node.GetName(), kTypeAttribute);
                return nullptr;
            }

            std::unique_ptr<Serializable> obj = SerializableRegistry::Instance().Create(typeName);
            if (!obj)
            {
                Log::Error("RttiXml::ReadPolymorphic: no Serializable registered for type '{}'.", typeName);
                return nullptr;
            }

            obj->Deserialize(node);
            return obj;
        }

        template <typename T>
        std::unique_ptr<T> ReadPolymorphicAs(const XMLNode &node)
        {
            static_assert(SF::RTTI::HasRtti<T>, "T needs SF_RTTI / SF_RTTI_BASE.");

            std::unique_ptr<Serializable> obj = ReadPolymorphic(node);
            if (!obj || !obj->RTTI_IsTypeOf(T::RTTI_Type()))
            {
                return nullptr;
            }
            return std::unique_ptr<T>(static_cast<T *>(obj.release()));
        }

        template <typename Container>
        void WritePolymorphicList(XMLNode &parent, const std::string &listElementName,
                                  const std::string &itemElementName, const Container &items)
        {
            XMLNode listNode = parent.AddChild(listElementName);
            for (const auto &itemPtr : items)
            {
                WritePolymorphic(listNode, itemElementName, *itemPtr);
            }
        }

        inline std::vector<std::unique_ptr<Serializable>> ReadPolymorphicList(
            const XMLNode &parent, const std::string &listElementName)
        {
            std::vector<std::unique_ptr<Serializable>> result;

            XMLNode listNode = parent.GetChild(listElementName);
            if (!listNode.IsValid())
            {
                return result;
            }

            for (XMLNode item : listNode.GetChildren())
            {
                if (auto obj = ReadPolymorphic(item))
                {
                    result.push_back(std::move(obj));
                }
            }
            return result;
        }
    }
}