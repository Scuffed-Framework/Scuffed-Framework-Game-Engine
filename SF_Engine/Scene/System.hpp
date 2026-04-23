#pragma once

#include <UtilityClasses/NoCopy.hpp>
#include <UtilityClasses/TypeInformation.hpp>

namespace SF::Engine
{
    class System : NoCopy
    {
    public:
        virtual ~System() = default;

        virtual void Update() = 0;

        bool IsEnabled() const
        {
            return enabled;
        }
        void SetEnabled(bool enable)
        {
            this->enabled = enable;
        }

    private:
        bool enabled = true;
    };

    template class TypeInformation<System>;
}
