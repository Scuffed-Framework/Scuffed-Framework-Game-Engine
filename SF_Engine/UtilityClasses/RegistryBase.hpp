#pragma once
#include <functional>
#include "NoCopy.hpp"

namespace SF::Engine
{
    template <typename Derived>
    class Registry : NoMove, NoCopy
    {
    public:
        static Derived &Get()
        {
            static Derived instance;
            return instance;
        }
    };
}