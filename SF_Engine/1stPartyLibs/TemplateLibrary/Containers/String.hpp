#pragma once
#include "AdvancedString.hpp"

namespace SFTL
{
    using string_view = AdvancedStringView<char>;

    using string    = AdvancedString<char>;
    using wstring   = AdvancedString<wchar_t>;
    using u8string  = AdvancedString<char8_t>;
    using u16string = AdvancedString<char16_t>;
    using u32string = AdvancedString<char32_t>;
}