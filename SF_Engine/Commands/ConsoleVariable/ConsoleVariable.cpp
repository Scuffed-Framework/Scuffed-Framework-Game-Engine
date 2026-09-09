#include "ConsoleVariable.hpp"

namespace SF::Engine
{
    template<typename V>
    ConsoleVariable<V>::ConsoleVariable(string mod, string nm, V defaultValue) :
        module(std::move(mod)), name(std::move(nm)), Value(defaultValue), LastValue(defaultValue)
    {
        ConsoleVariableRegistry::RegisterCVar(ConsoleVariable<V>::GetFullName(), this);
    }
} // namespace SF::Engine
