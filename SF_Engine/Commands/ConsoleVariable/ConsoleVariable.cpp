#include "ConsoleVariable.hpp"

namespace SF::Engine
{
    template<typename V>
    ConsoleVariable<V>::ConsoleVariable(string module, string name, V defaultValue) :
        module(std::move(module)), name(std::move(name)), Value(defaultValue), LastValue(defaultValue)
    {
        ConsoleVariableRegistry::RegisterCVar(ConsoleVariable<V>::GetFullName(), this);
    }
} // namespace SF::Engine
