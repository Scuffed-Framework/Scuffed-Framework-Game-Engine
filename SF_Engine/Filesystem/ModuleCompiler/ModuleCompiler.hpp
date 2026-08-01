#pragma once
#include <Engine/Module.hpp>

namespace SF::Engine
{
    // TODO:
    /*
    - Find external / custom Module (.xml)
    - look thru xml to find source files
    - ProcessBuilder to invoke a C++/C compiler
    - Export Module functions you would usually get by doing : ModuleRegistrar<class ... >
    */

    /*
    ModuleHeader: xml containing stage, etc...
    */
    struct ModuleHeader
    {
        Module::Stage stage;
        std::string name;
        TypeId typeId;
    };
}