#pragma once
#include <1stPartyLibs/TemplateLibrary/Types.hpp>
// Constraints:
// - Must be a multi platform solution, lets try to use as little platform includes as possible.
namespace SF::Engine
{
    using namespace SFTL;
    struct sockaddr_t;
    struct sockaddr_in
    {
        uint16 Family{};
        uint16 Port{};
        uint32 IPv4{};
        uint8 NOUSE_PAD[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    };
} // namespace SF::Engine
