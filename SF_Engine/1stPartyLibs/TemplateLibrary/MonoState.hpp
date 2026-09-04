#pragma once

namespace SFTL
{
    struct monostate { constexpr monostate() {} };

    constexpr bool operator==(monostate, monostate) noexcept { return true; }
    constexpr bool operator!=(monostate, monostate) noexcept { return false; }
    constexpr bool operator<(monostate, monostate) noexcept { return false; }
    constexpr bool operator>(monostate, monostate) noexcept { return false; }
    constexpr bool operator<=(monostate, monostate) noexcept { return true; }
    constexpr bool operator>=(monostate, monostate) noexcept { return true; }
}