#pragma once

#include <TemplateLibrary/Containers/String.hpp>
#include <Engine/Module.hpp>
#include <UtilityClasses/StreamFactory.hpp>

namespace SF::Engine
{
    class ConsoleVariableRegistry;

    template<typename V>
    struct ConsoleVariable : public Serializable
    {
        SF_RTTI_BASE(ConsoleVariable)
    public:
        ::SFTL::string module;
        ::SFTL::string name;
        V Value;
        V LastValue;

        ConsoleVariable(::SFTL::string mod, ::SFTL::string nm, V defaultValue);

        ~ConsoleVariable() override = default;

        [[nodiscard]] bool DidValueChange() const { return !(Value == LastValue); }

        [[nodiscard]] ::SFTL::string GetFullName() const
        {
            return module + "." + name;
        }

        void ChangeValue(const V& New)
        {
            LastValue = Value; // so DidValueChange() means something next frame
            Value = New;
        }

        void Serialize(XMLNode &node) const override
        {
            XMLNode CVar = node.AddChild("CVar");
            CVar.SetAttribute("Module", module);
            CVar.SetAttribute("Name", name);
            CVar.SetAttribute("Value", Value);
            CVar.SetAttribute("LastValue", LastValue);
        }

        void Deserialize(const XMLNode &node) override
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

        // Non-owning: CVars are expected to outlive the registry (static/global lifetime).
        inline static std::unordered_map<::SFTL::string, Serializable*> s_cvars;

    public:
        static void RegisterCVar(const ::SFTL::string& fullName, Serializable* cvar)
        {
            s_cvars[fullName] = cvar;
        }

        [[nodiscard]] static Serializable* Find(std::string_view fullName)
        {
            auto it = s_cvars.find(::SFTL::string(&fullName));
            return it != s_cvars.end() ? it->second : nullptr;
        }

        void Update() override {}

        [[nodiscard]] Stage GetStage() const override { return ModuleStage::Always; }
        [[nodiscard]] std::string_view GetName() const override { return RTTI_GetTypeName(); }

        void Serialize(XMLNode &node) const override
        {
            XMLNode registryNode = node.AddChild("ConsoleVariables");
            for (const auto& [fullName, cvar] : s_cvars)
                cvar->Serialize(registryNode);
        }

        void Deserialize(const XMLNode &node) override
        {
            XMLNode registryNode = node.GetChild("ConsoleVariables");
            for (XMLNode child = registryNode.FirstChild(); child; child = child.NextSibling())
            {
                ::SFTL::string mod, nm;
                child.GetAttribute("Module", mod);
                child.GetAttribute("Name", nm);
                if (Serializable* cvar = Find(mod + "." + nm))
                    cvar->Deserialize(registryNode); // each cvar re-finds its own <CVar> child
            }
        }

        Define_TypeId_Function(Module, ConsoleVariableRegistry)
    };
}