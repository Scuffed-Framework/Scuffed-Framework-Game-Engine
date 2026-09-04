/******************************************************************************/
/* Char.hpp                                                     */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Noah Lee                                        */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/
// SFTL/Char.hpp - A file containing char utilities.
#pragma once

#include "Filesystem/PosTypes.hpp"
#ifdef EOF
#undef EOF
#endif
#define EOF (-1)

namespace SFTL
{
	template <typename T, ::SFTL::size_type N>
	constexpr ::SFTL::size_type size(const T (&)[N]) noexcept
	{
		return N;
	}

	// Character classification utilities
	constexpr bool is_alpha(char c) noexcept
	{
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
	}

	constexpr bool is_digit(char c) noexcept
	{
		return c >= '0' && c <= '9';
	}

	constexpr bool is_alnum(char c) noexcept
	{
		return is_alpha(c) || is_digit(c);
	}

	constexpr bool is_space(char c) noexcept
	{
		return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
	}

	constexpr bool is_upper(char c) noexcept
	{
		return c >= 'A' && c <= 'Z';
	}

	constexpr bool is_lower(char c) noexcept
	{
		return c >= 'a' && c <= 'z';
	}

	constexpr bool is_hex(char c) noexcept
	{
		return is_digit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
	}

	constexpr bool is_printable(char c) noexcept
	{
		return c >= 32 && c <= 126;
	}

	// Character conversion utilities
	constexpr char to_upper(char c) noexcept
	{
		return is_lower(c) ? c - ('a' - 'A') : c;
	}

	constexpr char to_lower(char c) noexcept
	{
		return is_upper(c) ? c + ('a' - 'A') : c;
	}

	// String length (constexpr)
	constexpr ::SFTL::size_type strlen(const char *str) noexcept
	{
		::SFTL::size_type len = 0;
		while (str[len] != '\0')
			++len;
		return len;
	}

	// String comparison
	constexpr int strcmp(const char *s1, const char *s2) noexcept
	{
		while (*s1 && (*s1 == *s2))
		{
			++s1;
			++s2;
		}
		return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
	}

	constexpr int strncmp(const char *s1, const char *s2, ::SFTL::size_type n) noexcept
	{
		for (::SFTL::size_type i = 0; i < n; ++i)
		{
			if (s1[i] != s2[i])
				return static_cast<unsigned char>(s1[i]) - static_cast<unsigned char>(s2[i]);
			if (s1[i] == '\0')
				return 0;
		}
		return 0;
	}

	// Case-insensitive comparison
	constexpr int stricmp(const char *s1, const char *s2) noexcept
	{
		while (*s1 && (to_lower(*s1) == to_lower(*s2)))
		{
			++s1;
			++s2;
		}
		return to_lower(*s1) - to_lower(*s2);
	}

	// String search
	constexpr const char *strchr(const char *str, char ch) noexcept
	{
		while (*str)
		{
			if (*str == ch)
				return str;
			++str;
		}
		return ch == '\0' ? str : nullptr;
	}

	constexpr const char *strrchr(const char *str, char ch) noexcept
	{
		const char *last = nullptr;
		while (*str)
		{
			if (*str == ch)
				last = str;
			++str;
		}
		return (ch == '\0') ? str : last;
	}

	constexpr const char *strstr(const char *haystack, const char *needle) noexcept
	{
		if (*needle == '\0')
			return haystack;

		for (const char *h = haystack; *h; ++h)
		{
			const char *h_ptr = h;
			const char *n_ptr = needle;

			while (*h_ptr && *n_ptr && (*h_ptr == *n_ptr))
			{
				++h_ptr;
				++n_ptr;
			}

			if (*n_ptr == '\0')
				return h;
		}
		return nullptr;
	}

	// Character to digit conversion
	constexpr int char_to_digit(char c) noexcept
	{
		if (c >= '0' && c <= '9')
			return c - '0';
		if (c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		if (c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		return -1;
	}

	constexpr char digit_to_char(int digit, bool uppercase = true) noexcept
	{
		if (digit < 0 || digit > 35)
			return '\0';
		if (digit < 10)
			return '0' + digit;
		return (uppercase ? 'A' : 'a') + (digit - 10);
	}

	// Memory operations
	constexpr void *memset(void *dest, int ch, ::SFTL::size_type count) noexcept
	{
		auto *d = static_cast<unsigned char *>(dest);
		auto value = static_cast<unsigned char>(ch);
		for (::SFTL::size_type i = 0; i < count; ++i)
			d[i] = value;
		return dest;
	}

	constexpr void *memcpy(void *dest, const void *src, ::SFTL::size_type count) noexcept
	{
		auto *d = static_cast<unsigned char *>(dest);
		const auto *s = static_cast<const unsigned char *>(src);
		for (::SFTL::size_type i = 0; i < count; ++i)
			d[i] = s[i];
		return dest;
	}

	constexpr int memcmp(const void *lhs, const void *rhs, ::SFTL::size_type count) noexcept
	{
		const auto *l = static_cast<const unsigned char *>(lhs);
		const auto *r = static_cast<const unsigned char *>(rhs);
		for (::SFTL::size_type i = 0; i < count; ++i)
		{
			if (l[i] != r[i])
				return l[i] - r[i];
		}
		return 0;
	}

	constexpr void *memmove(void *dest, const void *src, ::SFTL::size_type count) noexcept
	{
		auto *d = static_cast<unsigned char *>(dest);
		const auto *s = static_cast<const unsigned char *>(src);
		if (d == s || count == 0) return dest;
		if (d < s)
			for (::SFTL::size_type i = 0; i < count; ++i) d[i] = s[i];
		else
			for (::SFTL::size_type i = count; i-- > 0;) d[i] = s[i];
		return dest;
	}

	inline constexpr int k_eof = -1;

	template <typename T> struct char_int_type;
	template <> struct char_int_type<char>     { using type = int; };
	template <> struct char_int_type<wchar_t>  { using type = int; };
	template <> struct char_int_type<char8_t>  { using type = unsigned int; };
	template <> struct char_int_type<char16_t> { using type = unsigned int; };
	template <> struct char_int_type<char32_t> { using type = unsigned int; };

	template <typename T>
	using char_int_type_t = typename char_int_type<T>::type;

	template <typename CharacterType>
	struct char_traits
	{
		using char_type = CharacterType;
		using int_type  = char_int_type_t<CharacterType>;

		static constexpr void assign(char_type &c1, char_type c2) noexcept { c1 = c2; }
		static constexpr bool eq(char_type c1, char_type c2) noexcept { return c1 == c2; }
		static constexpr bool lt(char_type c1, char_type c2) noexcept { return c1 < c2; }

		static constexpr int compare(const char_type *s1, const char_type *s2, ::SFTL::size_type n) noexcept
		{
			if constexpr (sizeof(char_type) == 1)
				return n ? ::SFTL::memcmp(s1, s2, n) : 0;
			else
			{
				for (::SFTL::size_type i = 0; i < n; ++i)
				{
					if (lt(s1[i], s2[i])) return -1;
					if (lt(s2[i], s1[i])) return 1;
				}
				return 0;
			}
		}

		static constexpr ::SFTL::size_type length(const char_type *s) noexcept
		{
			if constexpr (sizeof(char_type) == 1)
				return ::SFTL::strlen(reinterpret_cast<const char *>(s));
			else
			{
				::SFTL::size_type i = 0;
				while (!eq(s[i], char_type{})) ++i;
				return i;
			}
		}

		static constexpr const char_type *find(const char_type *s, ::SFTL::size_type n, char_type a) noexcept
		{
			for (::SFTL::size_type i = 0; i < n; ++i)
				if (eq(s[i], a)) return s + i;
			return nullptr;
		}

		static constexpr char_type *move(char_type *dst, const char_type *src, ::SFTL::size_type n) noexcept
		{
			if (n == 0) return dst;
			return static_cast<char_type *>(::SFTL::memmove(dst, src, n * sizeof(char_type)));
		}

		static constexpr char_type *copy(char_type *dst, const char_type *src, ::SFTL::size_type n) noexcept
		{
			if (n == 0) return dst;
			return static_cast<char_type *>(::SFTL::memcpy(dst, src, n * sizeof(char_type)));
		}

		static constexpr char_type *assign(char_type *s, ::SFTL::size_type n, char_type a) noexcept
		{
			if constexpr (sizeof(char_type) == 1)
			{
				::SFTL::memset(s, static_cast<int>(a), n);
			}
			else
			{
				for (::SFTL::size_type i = 0; i < n; ++i) s[i] = a;
			}
			return s;
		}

		static constexpr char_type to_char_type(int_type c) noexcept { return static_cast<char_type>(c); }
		static constexpr int_type  to_int_type(char_type c) noexcept { return static_cast<int_type>(c); }
		static constexpr bool eq_int_type(int_type a, int_type b) noexcept { return a == b; }
		static constexpr int_type eof() noexcept { return static_cast<int_type>(::SFTL::k_eof); }
		static constexpr int_type not_eof(int_type c) noexcept { return eq_int_type(c, eof()) ? int_type{} : c; }
	};
}