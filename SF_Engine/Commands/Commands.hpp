#pragma once
#include <UtilityClasses/RegistryBase.hpp>
#include <string>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <concepts>

namespace SF::Engine
{
    struct Commandlet
    {
        std::string name;
        std::vector<std::string> args;
        virtual void Execute() = 0;
        virtual ~Commandlet() = default;
    };

    class CommandletRegistry : public Registry<CommandletRegistry>
    {
        friend class Registry<CommandletRegistry>;

    public:
        template <typename T>
        std::shared_ptr<Commandlet> Register()
        {
            static_assert(std::derived_from<T, Commandlet>, "T must derive from Commandlet");

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

        std::shared_ptr<Commandlet> FindByName(const std::string &name) const
        {
            for (auto &[id, cmd] : commandlets_)
                if (cmd->name == name)
                    return cmd;
            return nullptr;
        }

    private:
        std::unordered_map<std::type_index, std::shared_ptr<Commandlet>> commandlets_;
    };
}