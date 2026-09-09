#pragma once
#include <UtilityClasses/RegistryBase.hpp>
#include <concepts>
#include <memory>
#include <ranges>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>


namespace SF::Engine
{
    using namespace std;
    struct Commandlet
    {
        string name;
        vector<string> args;
        virtual void Execute() = 0;
        virtual ~Commandlet()  = default;
    };

    class CommandletRegistry : public Registry<CommandletRegistry>
    {
        friend class Registry<CommandletRegistry>;

    public:
        template<typename T>
        shared_ptr<Commandlet> Register()
        {
            static_assert(derived_from<T, Commandlet>, "T must derive from Commandlet");

            auto id = type_index(typeid(T));
            if (const auto it = commandlets_.find(id); it != commandlets_.end())
                return it->second;

            auto cmd = make_shared<T>();
            commandlets_.emplace(id, cmd);
            return cmd;
        }

        void Unregister(shared_ptr<Commandlet> cmd)
        {
            erase_if(commandlets_, [&cmd](const auto &entry) // capture cmd
                     { return entry.second == cmd; });
        }

        shared_ptr<Commandlet> FindByName(const string &name) const
        {
            for (const auto &cmd: commandlets_ | views::values)
                if (cmd->name == name)
                    return cmd;
            return nullptr;
        }

        unordered_map<type_index, shared_ptr<Commandlet>> commandlets_;
    };
} // namespace SF::Engine
