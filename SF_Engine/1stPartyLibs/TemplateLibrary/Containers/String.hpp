#pragma once
#include "AdvancedString.hpp"

namespace SFTL
{
    using string_view = AdvancedStringView<char>;

    using string = AdvancedString<char>;
    DEFINE_STRING_LITERAL_OPERATOR(string, _str)

    using wstring = AdvancedString<wchar_t>;
    DEFINE_STRING_LITERAL_OPERATOR(string, _wstr)

    using u8string = AdvancedString<char8_t>;
    DEFINE_STRING_LITERAL_OPERATOR(string, _u8str)

    using u16string = AdvancedString<char16_t>;
    DEFINE_STRING_LITERAL_OPERATOR(string, _u16str)

    using u32string = AdvancedString<char32_t>;
    DEFINE_STRING_LITERAL_OPERATOR(string, _u32str)
    
} // namespace SFTL
