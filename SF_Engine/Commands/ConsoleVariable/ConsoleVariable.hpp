#pragma once
#include <charconv>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

#include <TemplateLibrary/Containers/String.hpp>
#include <Engine/Module.hpp>
#include <UtilityClasses/StreamFactory.hpp>
#include <LowLevel/XML/XMLModule.hpp>

namespace SF::Engine
{
    inline constexpr auto CommandWindowConsoleVariablePrefix = "CVar::";

    class ConsoleVariableRegistry;
    struct IConsoleVariable : public Serializable
    {
    public:
        ~IConsoleVariable() override = default;

        [[nodiscard]] virtual ::SFTL::string GetFullName() const = 0;
        [[nodiscard]] virtual bool DidValueChange() const = 0;

        virtual bool SetValueFromString(::SFTL::string_view str) = 0;
        [[nodiscard]] virtual ::SFTL::string ValueToString() const = 0;
    };

    template<typename V>
    struct ConsoleVariable : public IConsoleVariable
    {
        SF_RTTI(IConsoleVariable, ConsoleVariable)

    public:
        ::SFTL::string module;
        ::SFTL::string name;
        V Value;
        V LastValue;

        ConsoleVariable(::SFTL::string mod, ::SFTL::string nm, V defaultValue);
        ~ConsoleVariable() override = default;

        [[nodiscard]] bool DidValueChange() const override
        {
            return !(Value == LastValue);
        }

        [[nodiscard]] ::SFTL::string GetFullName() const override
        {
            return module + "." + name;
        }

        void ChangeValue(const V& New)
        {
            LastValue = Value; // so DidValueChange() means something next frame
            Value = New;
        }

        bool SetValueFromString(::SFTL::string_view str) override
        {
            if constexpr (std::is_same_v<V, bool>)
            {
                if (str == "true" || str == "1")
                {
                    ChangeValue(true);
                    return true;
                }
                if (str == "false" || str == "0")
                {
                    ChangeValue(false);
                    return true;
                }
                return false;
            }
            else if constexpr (std::is_arithmetic_v<V>)
            {
                V parsed{};
                auto [ptr, ec] = std::from_chars(str.Data(), str.Data() + str.Size(), parsed);
                if (ec != std::errc{})
                    return false;
                ChangeValue(parsed);
                return true;
            }
            else if constexpr (std::is_constructible_v<::SFTL::string, ::SFTL::string_view>)
            {
                // Assumes V is (or is constructible from) SFTL::string.
                ChangeValue(::SFTL::string(str.Data()));
                return true;
            }
            else
            {
                static_assert(!sizeof(V*), "SetValueFromString not implemented for this CVar type");
                return false;
            }
        }

        [[nodiscard]] ::SFTL::string ValueToString() const override
        {
            if constexpr (std::is_same_v<V, bool>)
            {
                return Value ? "true" : "false";
            }
            else if constexpr (std::is_arithmetic_v<V>)
            {
                return ::SFTL::string(std::to_string(Value));
            }
            else
            {
                return Value; // assumes SFTL::string-compatible
            }
        }

        void Serialize(XMLNode& node) const override
        {
            XMLNode CVar = node.AddChild("CVar");
            CVar.SetAttribute("Module", module);
            CVar.SetAttribute("Name", name);
            CVar.SetAttribute("Value", Value);
            CVar.SetAttribute("LastValue", LastValue);
        }

        void Deserialize(const XMLNode& node) override
        {
            XMLNode CVar = node.GetChild("CVar");
            CVar.GetAttribute("Module", module);
            CVar.GetAttribute("Name", name);
            CVar.GetAttribute("Value", Value);
            CVar.GetAttribute("LastValue", LastValue);
        }
    };

    class ConsoleVariableRegistry : public ModuleRegistrar<ConsoleVariableRegistry>
    {
        static inline bool reg = Register(ModuleStage::Always, Requires<>{});
        SF_RTTI(Module, ConsoleVariableRegistry)

        inline static std::unordered_map<::SFTL::string, IConsoleVariable*> s_cvars;

    public:
        static void RegisterCVar(const ::SFTL::string& fullName, IConsoleVariable* cvar)
        {
            s_cvars[fullName] = cvar;
        }

        [[nodiscard]] static IConsoleVariable* Find(::SFTL::string_view fullName)
        {
            auto it = s_cvars.find(::SFTL::string(fullName)); // :(
            return it != s_cvars.end() ? it->second : nullptr;
        }

        bool Initialize() override { return true; }
        void Update() override {}

        [[nodiscard]] Stage GetStage() const override { return ModuleStage::Always; }
        [[nodiscard]] ::std::string_view GetName() const override { return RTTI_GetTypeName(); } // todo: add overload for sftl one

        // NOLINTBEGIN(readability-convert-member-functions-to-static)
        void Serialize(XMLNode& node) const
        {
            XMLNode registryNode = node.AddChild("ConsoleVariables");
            for (const auto& [fullName, cvar] : s_cvars)
                cvar->Serialize(registryNode);
        }

        void Deserialize(const XMLNode& node)
        {
            XMLNode registryNode = node.GetChild("ConsoleVariables");
            for (XMLNode child = registryNode.GetFirstChild(); child.IsValid(); child = child.GetNextSibling())
            {
                ::SFTL::string mod, nm;
                child.GetAttribute("Module", mod);
                child.GetAttribute("Name", nm);
                if (IConsoleVariable* cvar = Find(mod + "." + nm))
                    cvar->Deserialize(registryNode); // each cvar re-finds its own <CVar> child
            }
        }
        // NOLINTEND(readability-convert-member-functions-to-static)

        Define_TypeId_Function(Module, ConsoleVariableRegistry)
    };
}