#pragma once

#include <typeindex>

#include <UtilityClasses/NoCopy.hpp>

namespace SF::Engine
{
    /**
     * @brief A managed resource object. Implementations contain Create functions that can take a node object or pass parameters to the constructor.
     */
    class Resource : NoCopy
    {
    public:
        Resource() = default;
        virtual ~Resource() = default;

        virtual std::type_index GetTypeIndex() const = 0;
    };
}