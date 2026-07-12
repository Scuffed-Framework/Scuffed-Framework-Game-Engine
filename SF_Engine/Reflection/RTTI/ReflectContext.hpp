#pragma once

#include "RTTI.hpp"

namespace SF::RTTI
{
    class ReflectContext
    {
        SF_RTTI_BASE(ReflectContext)
    public:
        virtual ~ReflectContext() = default;
    };
}
