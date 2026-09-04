#include "ConsoleVariable.hpp"

namespace SF::Engine
{
    template<typename V>
   ConsoleVariable<V>::ConsoleVariable(::SFTL::string mod, ::SFTL::string nm, V defaultValue)
       : module(std::move(mod))
       , name(std::move(nm))
       , Value(defaultValue)
       , LastValue(defaultValue)
    {
        ConsoleVariableRegistry::RegisterCVar(GetFullName(), this);
    }
}
