#pragma once

#include <array>
#include <cstdint>
#include <variant>

namespace SF::Engine
{
    struct IpV4Address
    {
        std::array<uint8_t, 4> Address{};
    };

    struct IpV6Address
    {
        std::array<uint8_t, 16> Address{};
    };

    struct SocketAddress
    {
        std::variant<IpV4Address, IpV6Address> Address;
        uint16_t Port{};
    };
} // namespace SF::Engine
