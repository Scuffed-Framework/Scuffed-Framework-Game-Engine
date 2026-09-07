#pragma once
#include <1stPartyLibs/TemplateLibrary/Containers/String.hpp>
#include <UtilityClasses/RegistryBase.hpp>
#include <concepts>
#include <memory>
#include <ranges>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <1stPartyLibs/TemplateLibrary/DynamicArray.hpp>

namespace SF::Engine
{
    struct Commandlet
    {
        ::SFTL::string name;
        ::SFTL::DynamicArray<::SFTL::string> args;
        virtual void Execute() = 0;
        virtual ~Commandlet()  = default;
    };

    class CommandletRegistry : public Registry<CommandletRegistry>
    {
        friend class Registry<CommandletRegistry>;

    public:
        template<typename T>
        std::shared_ptr<Commandlet> Register()
        {
            static_assert(::SFTL::derived_from<T, Commandlet>, "T must derive from Commandlet");

            auto id = std::type_index(typeid(T));
            if (auto it = commandlets_.find(id); it != commandlets_.end())
                return it->second;

            auto cmd = std::make_shared<T>(); // consistent name
            commandlets_.emplace(id, cmd);
            return cmd;
        }

        void Unregister(std::shared_ptr<Commandlet> cmd) // match the type
        {
            std::erase_if(commandlets_, [&cmd](const auto &entry) // capture cmd
                          { return entry.second == cmd; });
        }

        std::shared_ptr<Commandlet> FindByName(const ::SFTL::string &name) const
        {
            for (const auto &cmd: commandlets_ | std::views::values)
                if (cmd->name == name)
                    return cmd;
            return nullptr;
        }

    private:
        std::unordered_map<std::type_index, std::shared_ptr<Commandlet>> commandlets_;
    };
} // namespace SF::Engine
