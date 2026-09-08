#pragma once

#include "Locale.hpp"

namespace SFTL
{
    constexpr int utf8_encode(char32_t cp, char *out) noexcept
    {
        if (cp <= 0x7F)
        {
            out[0] = static_cast<char>(cp);
            return 1;
        }
        if (cp <= 0x7FF)
        {
            out[0] = static_cast<char>(0xC0 | (cp >> 6));
            out[1] = static_cast<char>(0x80 | (cp & 0x3F));
            return 2;
        }
        if (cp >= 0xD800 && cp <= 0xDFFF)
            return 0;
        if (cp <= 0xFFFF)
        {
            out[0] = static_cast<char>(0xE0 | (cp >> 12));
            out[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out[2] = static_cast<char>(0x80 | (cp & 0x3F));
            return 3;
        }
        if (cp <= 0x10FFFF)
        {
            out[0] = static_cast<char>(0xF0 | (cp >> 18));
            out[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out[3] = static_cast<char>(0x80 | (cp & 0x3F));
            return 4;
        }
        return 0;
    }

    constexpr int utf8_seq_length(unsigned char lead) noexcept
    {
        if ((lead & 0x80) == 0x00)
            return 1;
        if ((lead & 0xE0) == 0xC0)
            return 2;
        if ((lead & 0xF0) == 0xE0)
            return 3;
        if ((lead & 0xF8) == 0xF0)
            return 4;
        return 0;
    }

    template<typename Result>
    constexpr Result utf8_decode(const char *first, const char *last, char32_t &cp, int &consumed, Result ok,
                                 Result partial, Result error) noexcept
    {
        if (first >= last)
            return partial;

        unsigned char lead = static_cast<unsigned char>(*first);
        int len            = utf8_seq_length(lead);
        if (len == 0)
            return error;
        if (last - first < len)
            return partial;

        char32_t value = (len == 1) ? lead : (len == 2) ? (lead & 0x1F) : (len == 3) ? (lead & 0x0F) : (lead & 0x07);
        for (int i = 1; i < len; ++i)
        {
            unsigned char c = static_cast<unsigned char>(first[i]);
            if ((c & 0xC0) != 0x80)
                return error;
            value = (value << 6) | (c & 0x3F);
        }

        constexpr char32_t min_value[5] = {0, 0, 0x80, 0x800, 0x10000};
        if (value < min_value[len])
            return error; // overlong encoding
        if (value >= 0xD800 && value <= 0xDFFF)
            return error; // encoded surrogate
        if (value > 0x10FFFF)
            return error;

        cp       = value;
        consumed = len;
        return ok;
    }

    constexpr bool utf16_is_high_surrogate(char32_t u) noexcept { return u >= 0xD800 && u <= 0xDBFF; }
    constexpr bool utf16_is_low_surrogate(char32_t u) noexcept { return u >= 0xDC00 && u <= 0xDFFF; }

    class codecvt_base
    {
    public:
        enum result
        {
            ok,
            partial,
            error,
            noconv
        };
    };

    template<typename UcharT>
    constexpr codecvt_base::result utf8_to_ucs(const char *from, const char *from_end, const char *&from_next,
                                               UcharT *to, UcharT *to_end, UcharT *&to_next) noexcept
    {
        using result   = codecvt_base::result;
        UcharT *out    = to;
        const char *in = from;

        while (in < from_end && out < to_end)
        {
            char32_t cp;
            int consumed;
            result r = utf8_decode(in, from_end, cp, consumed, result::ok, result::partial, result::error);
            if (r != result::ok)
            {
                from_next = in;
                to_next   = out;
                return (in == from) ? r : result::ok;
            }

            if constexpr (sizeof(UcharT) == 2) // UTF-16 target: may need a surrogate pair
            {
                if (cp <= 0xFFFF)
                {
                    *out++ = static_cast<UcharT>(cp);
                } else
                {
                    if (to_end - out < 2)
                        break; // not enough room; stop before consuming the input
                    cp -= 0x10000;
                    *out++ = static_cast<UcharT>(0xD800 + (cp >> 10));
                    *out++ = static_cast<UcharT>(0xDC00 + (cp & 0x3FF));
                }
            } else
            {
                *out++ = static_cast<UcharT>(cp);
            }
            in += consumed;
        }
        from_next = in;
        to_next   = out;
        return (in == from_end) ? result::ok : result::partial;
    }

    template<typename UcharT>
    constexpr codecvt_base::result ucs_to_utf8(const UcharT *from, const UcharT *from_end, const UcharT *&from_next,
                                               char *to, char *to_end, char *&to_next) noexcept
    {
        using result     = codecvt_base::result;
        const UcharT *in = from;
        char *out        = to;

        while (in < from_end)
        {
            char32_t cp;
            const UcharT *next_in = in;

            if constexpr (sizeof(UcharT) == 2)
            {
                char32_t unit = static_cast<char32_t>(static_cast<uint16>(*in));
                if (utf16_is_high_surrogate(unit))
                {
                    if (from_end - in < 2)
                    {
                        from_next = in;
                        to_next   = out;
                        return result::partial;
                    }
                    char32_t low = static_cast<char32_t>(static_cast<uint16>(in[1]));
                    if (!utf16_is_low_surrogate(low))
                    {
                        from_next = in;
                        to_next   = out;
                        return result::error;
                    }
                    cp      = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
                    next_in = in + 2;
                } else if (utf16_is_low_surrogate(unit))
                {
                    from_next = in;
                    to_next   = out;
                    return result::error;
                } else
                {
                    cp      = unit;
                    next_in = in + 1;
                }
            } else
            {
                cp      = static_cast<char32_t>(*in);
                next_in = in + 1;
            }

            char buf[4];
            int n = utf8_encode(cp, buf);
            if (n == 0)
            {
                from_next = in;
                to_next   = out;
                return result::error;
            }
            if (to_end - out < n)
            {
                from_next = in;
                to_next   = out;
                return result::partial;
            }
            for (int i = 0; i < n; ++i)
                *out++ = buf[i];
            in = next_in;
        }
        from_next = in;
        to_next   = out;
        return result::ok;
    }

    // Counts how many source bytes/units decode into at most max
    // destination units, for the "UcharT <-> UTF-8" do_length bodies.
    template<typename UcharT>
    constexpr int utf8_count_length(const char *from, const char *end, size_t max) noexcept
    {
        using result    = codecvt_base::result;
        size_t produced = 0;
        const char *p   = from;
        while (p < end && produced < max)
        {
            char32_t cp;
            int consumed;
            if (utf8_decode(p, end, cp, consumed, result::ok, result::partial, result::error) != result::ok)
                break;
            p += consumed;
            produced += (sizeof(UcharT) == 2 && cp > 0xFFFF) ? 2 : 1;
        }
        return static_cast<int>(p - from);
    }

    template<typename Internal, typename External, typename State>
    class codecvt_abstract_base : public locale::facet, public codecvt_base
    {
    public:
        typedef result result;
        typedef Internal intern_type;
        typedef External extern_type;
        typedef State state_type;

        constexpr result out(state_type &state, const intern_type *from, const intern_type *from_end,
                             const intern_type *&from_next, extern_type *to, extern_type *to_end,
                             extern_type *&to_next) const
        {
            return this->do_out(state, from, from_end, from_next, to, to_end, to_next);
        }

        constexpr result unshift(state_type &state, extern_type *to, extern_type *to_end, extern_type *&to_next) const
        {
            return this->do_unshift(state, to, to_end, to_next);
        }

        constexpr result in(state_type &state, const extern_type *from, const extern_type *from_end,
                            const extern_type *&from_next, intern_type *to, intern_type *to_end,
                            intern_type *&to_next) const
        {
            return this->do_in(state, from, from_end, from_next, to, to_end, to_next);
        }

        constexpr int encoding() const noexcept { return this->do_encoding(); }

        constexpr bool always_noconv() const noexcept { return this->do_always_noconv(); }

        constexpr int length(state_type &state, const extern_type *from, const extern_type *end, size_t max) const
        {
            return this->do_length(state, from, end, max);
        }

        constexpr int max_length() const noexcept { return this->do_max_length(); }

    protected:
        explicit codecvt_abstract_base(size_t refs = 0) : locale::facet(refs) {}

        virtual ~codecvt_abstract_base() {}

        virtual constexpr result do_out(state_type &state, const intern_type *from, const intern_type *from_end,
                                        const intern_type *&from_next, extern_type *to, extern_type *to_end,
                                        extern_type *&to_next) const = 0;

        virtual constexpr result do_unshift(state_type &state, extern_type *to, extern_type *to_end,
                                            extern_type *&to_next) const = 0;

        virtual constexpr result do_in(state_type &state, const extern_type *from, const extern_type *from_end,
                                       const extern_type *&from_next, intern_type *to, intern_type *to_end,
                                       intern_type *&to_next) const = 0;

        virtual constexpr int do_encoding() const noexcept = 0;

        virtual constexpr bool do_always_noconv() const noexcept = 0;

        virtual constexpr int do_length(state_type &, const extern_type *from, const extern_type *end,
                                        size_t max) const = 0;

        virtual constexpr int do_max_length() const noexcept = 0;
    };

    template<typename Internal, typename External, typename State>
    class codecvt : public codecvt_abstract_base<Internal, External, State>
    {
    public:
        typedef codecvt_base::result result;
        typedef Internal intern_type;
        typedef External extern_type;
        typedef State state_type;

    protected:
        clocale _M_c_locale_codecvt;

    public:
        static locale::id id;

        explicit codecvt(size_t refs = 0) :
            codecvt_abstract_base<Internal, External, State>(refs), _M_c_locale_codecvt(0)
        {
        }

        // Header-only, like char_traits: defined right here instead of
        // out-of-line in a .cpp, since it's a template anyway.
        explicit codecvt(clocale cloc, size_t refs = 0) :
            codecvt_abstract_base<Internal, External, State>(refs), _M_c_locale_codecvt(this->clone_c_locale(cloc))
        {
        }

    protected:
        virtual ~codecvt() {}

        virtual result do_out(state_type &state, const intern_type *from, const intern_type *from_end,
                              const intern_type *&from_next, extern_type *to, extern_type *to_end,
                              extern_type *&to_next) const;

        virtual result do_unshift(state_type &state, extern_type *to, extern_type *to_end, extern_type *&to_next) const;

        virtual result do_in(state_type &state, const extern_type *from, const extern_type *from_end,
                             const extern_type *&from_next, intern_type *to, intern_type *to_end,
                             intern_type *&to_next) const;

        virtual int do_encoding() const noexcept;

        virtual bool do_always_noconv() const noexcept;

        virtual int do_length(state_type &, const extern_type *from, const extern_type *end, size_t max) const;

        virtual int do_max_length() const noexcept;
    };

    template<typename Internal, typename External, typename State>
    locale::id codecvt<Internal, External, State>::id;

    // ------------------------------------------------------------------
    // codecvt<char, char, mbstate_type>  -- identity conversion
    // Everything defined inline in the class body, exactly like
    // char_traits: no forward declaration + out-of-line body split.
    // ------------------------------------------------------------------
    template<>
    class codecvt<char, char, mbstate_type> : public codecvt_abstract_base<char, char, mbstate_type>
    {
    public:
        typedef char intern_type;
        typedef char extern_type;
        typedef mbstate_type state_type;

    protected:
        clocale _M_c_locale_codecvt;

    public:
        static locale::id id;

        explicit codecvt(size_t refs = 0) :
            codecvt_abstract_base<char, char, mbstate_type>(refs), _M_c_locale_codecvt(get_c_locale())
        {
        }

        explicit codecvt(clocale cloc, size_t refs = 0) :
            codecvt_abstract_base<char, char, mbstate_type>(refs), _M_c_locale_codecvt(clone_c_locale(cloc))
        {
        }

    protected:
        virtual ~codecvt() { destroy_c_locale(_M_c_locale_codecvt); }

        constexpr result do_out(state_type &, const intern_type *from, const intern_type *from_end,
                                const intern_type *&from_next, extern_type *to, extern_type *to_end,
                                extern_type *&to_next) const override
        {
            size_type n = ::SFTL::size_type((from_end - from) < (to_end - to) ? (from_end - from) : (to_end - to));
            ::SFTL::memcpy(to, from, n);
            from_next = from + n;
            to_next   = to + n;
            return (from_next == from_end) ? ok : partial;
        }

        constexpr result do_unshift(state_type &, extern_type *to, extern_type *, extern_type *&to_next) const override
        {
            to_next = to;
            return noconv;
        }

        constexpr result do_in(state_type &, const extern_type *from, const extern_type *from_end,
                               const extern_type *&from_next, intern_type *to, intern_type *to_end,
                               intern_type *&to_next) const override
        {
            size_type n = ::SFTL::size_type((from_end - from) < (to_end - to) ? (from_end - from) : (to_end - to));
            ::SFTL::memcpy(to, from, n);
            from_next = from + n;
            to_next   = to + n;
            return (from_next == from_end) ? ok : partial;
        }

        constexpr int do_encoding() const noexcept override { return 1; }
        constexpr bool do_always_noconv() const noexcept override { return true; }

        constexpr int do_length(state_type &, const extern_type *from, const extern_type *end,
                                size_t max) const override
        {
            size_type n = static_cast<size_type>(end - from);
            return static_cast<int>(n < max ? n : max);
        }

        constexpr int do_max_length() const noexcept override { return 1; }
    };

    // ------------------------------------------------------------------
    // codecvt<wchar_t, char, mbstate_type>  -- wchar_t <-> UTF-8
    // Treats wchar_t as UTF-32 (Linux/macOS) or UTF-16 (Windows) based
    // on sizeof(wchar_t), reusing the ucs_to_utf8/utf8_to_ucs helpers.
    // ------------------------------------------------------------------
    template<>
    class codecvt<wchar_t, char, mbstate_type> : public codecvt_abstract_base<wchar_t, char, mbstate_type>
    {
    public:
        typedef wchar_t intern_type;
        typedef char extern_type;
        typedef mbstate_type state_type;

    protected:
        clocale _M_c_locale_codecvt;

    public:
        static locale::id id;

        explicit codecvt(size_t refs = 0) :
            codecvt_abstract_base<wchar_t, char, mbstate_type>(refs), _M_c_locale_codecvt(get_c_locale())
        {
        }

        explicit codecvt(clocale cloc, size_t refs = 0) :
            codecvt_abstract_base<wchar_t, char, mbstate_type>(refs), _M_c_locale_codecvt(clone_c_locale(cloc))
        {
        }

    protected:
        virtual ~codecvt() { destroy_c_locale(_M_c_locale_codecvt); }

        result do_out(state_type &, const intern_type *from, const intern_type *from_end, const intern_type *&from_next,
                      extern_type *to, extern_type *to_end, extern_type *&to_next) const override
        {
            if constexpr (sizeof(wchar_t) == 2)
                return ucs_to_utf8(reinterpret_cast<const char16_t *>(from),
                                   reinterpret_cast<const char16_t *>(from_end),
                                   reinterpret_cast<const char16_t *&>(from_next), to, to_end, to_next);
            else
                return ucs_to_utf8(reinterpret_cast<const char32_t *>(from),
                                   reinterpret_cast<const char32_t *>(from_end),
                                   reinterpret_cast<const char32_t *&>(from_next), to, to_end, to_next);
        }

        constexpr result do_unshift(state_type &, extern_type *to, extern_type *, extern_type *&to_next) const override
        {
            to_next = to;
            return noconv;
        }

        result do_in(state_type &, const extern_type *from, const extern_type *from_end, const extern_type *&from_next,
                     intern_type *to, intern_type *to_end, intern_type *&to_next) const override
        {
            if constexpr (sizeof(wchar_t) == 2)
                return utf8_to_ucs(from, from_end, from_next, reinterpret_cast<char16_t *>(to),
                                   reinterpret_cast<char16_t *>(to_end), reinterpret_cast<char16_t *&>(to_next));
            else
                return utf8_to_ucs(from, from_end, from_next, reinterpret_cast<char32_t *>(to),
                                   reinterpret_cast<char32_t *>(to_end), reinterpret_cast<char32_t *&>(to_next));
        }

        constexpr int do_encoding() const noexcept override { return 0; } // stateless, variable width
        constexpr bool do_always_noconv() const noexcept override { return false; }

        constexpr int do_length(state_type &, const extern_type *from, const extern_type *end,
                                size_t max) const override
        {
            if constexpr (sizeof(wchar_t) == 2)
                return utf8_count_length<char16_t>(from, end, max);
            else
                return utf8_count_length<char32_t>(from, end, max);
        }

        constexpr int do_max_length() const noexcept override { return 4; }
    };

    // ------------------------------------------------------------------
    // codecvt<char16_t, char, mbstate_type>  -- UTF-16 <-> UTF-8
    // ------------------------------------------------------------------
    template<>
    class codecvt<char16_t, char, mbstate_type> : public codecvt_abstract_base<char16_t, char, mbstate_type>
    {
    public:
        typedef char16_t intern_type;
        typedef char extern_type;
        typedef mbstate_type state_type;

    public:
        static locale::id id;

        explicit codecvt(size_t refs = 0) : codecvt_abstract_base<char16_t, char, mbstate_type>(refs) {}

    protected:
        virtual ~codecvt() {}

        constexpr result do_out(state_type &, const intern_type *from, const intern_type *from_end,
                                const intern_type *&from_next, extern_type *to, extern_type *to_end,
                                extern_type *&to_next) const override
        {
            return ucs_to_utf8(from, from_end, from_next, to, to_end, to_next);
        }

        constexpr result do_unshift(state_type &, extern_type *to, extern_type *, extern_type *&to_next) const override
        {
            to_next = to;
            return noconv;
        }

        constexpr result do_in(state_type &, const extern_type *from, const extern_type *from_end,
                               const extern_type *&from_next, intern_type *to, intern_type *to_end,
                               intern_type *&to_next) const override
        {
            return utf8_to_ucs(from, from_end, from_next, to, to_end, to_next);
        }

        constexpr int do_encoding() const noexcept override { return 0; }
        constexpr bool do_always_noconv() const noexcept override { return false; }

        constexpr int do_length(state_type &, const extern_type *from, const extern_type *end,
                                size_t max) const override
        {
            return utf8_count_length<char16_t>(from, end, max);
        }

        constexpr int do_max_length() const noexcept override { return 4; }
    };

    // ------------------------------------------------------------------
    // codecvt<char32_t, char, mbstate_type>  -- UTF-32 <-> UTF-8
    // ------------------------------------------------------------------
    template<>
    class codecvt<char32_t, char, mbstate_type> : public codecvt_abstract_base<char32_t, char, mbstate_type>
    {
    public:
        typedef char32_t intern_type;
        typedef char extern_type;
        typedef mbstate_type state_type;

    public:
        static locale::id id;

        explicit codecvt(size_t refs = 0) : codecvt_abstract_base<char32_t, char, mbstate_type>(refs) {}

    protected:
        virtual ~codecvt() {}

        constexpr result do_out(state_type &, const intern_type *from, const intern_type *from_end,
                                const intern_type *&from_next, extern_type *to, extern_type *to_end,
                                extern_type *&to_next) const override
        {
            return ucs_to_utf8(from, from_end, from_next, to, to_end, to_next);
        }

        constexpr result do_unshift(state_type &, extern_type *to, extern_type *, extern_type *&to_next) const override
        {
            to_next = to;
            return noconv;
        }

        constexpr result do_in(state_type &, const extern_type *from, const extern_type *from_end,
                               const extern_type *&from_next, intern_type *to, intern_type *to_end,
                               intern_type *&to_next) const override
        {
            return utf8_to_ucs(from, from_end, from_next, to, to_end, to_next);
        }

        constexpr int do_encoding() const noexcept override { return 0; }
        constexpr bool do_always_noconv() const noexcept override { return false; }

        constexpr int do_length(state_type &, const extern_type *from, const extern_type *end,
                                size_t max) const override
        {
            return utf8_count_length<char32_t>(from, end, max);
        }

        constexpr int do_max_length() const noexcept override { return 4; }
    };

    // ------------------------------------------------------------------
    // codecvt<char16_t, char8_t, mbstate_type> and
    // codecvt<char32_t, char8_t, mbstate_type>
    // Same algorithms as above, external type is char8_t instead of
    // char; reinterpret_cast is safe since char8_t and char share
    // representation.
    // ------------------------------------------------------------------
    template<>
    class codecvt<char16_t, char8_t, mbstate_type> : public codecvt_abstract_base<char16_t, char8_t, mbstate_type>
    {
    public:
        typedef char16_t intern_type;
        typedef char8_t extern_type;
        typedef mbstate_type state_type;

    public:
        static locale::id id;

        explicit codecvt(size_t refs = 0) : codecvt_abstract_base<char16_t, char8_t, mbstate_type>(refs) {}

    protected:
        virtual ~codecvt() {}

        result do_out(state_type &, const intern_type *from, const intern_type *from_end, const intern_type *&from_next,
                      extern_type *to, extern_type *to_end, extern_type *&to_next) const override
        {
            return ucs_to_utf8(from, from_end, from_next, reinterpret_cast<char *>(to),
                               reinterpret_cast<char *>(to_end), reinterpret_cast<char *&>(to_next));
        }

        constexpr result do_unshift(state_type &, extern_type *to, extern_type *, extern_type *&to_next) const override
        {
            to_next = to;
            return noconv;
        }

        result do_in(state_type &, const extern_type *from, const extern_type *from_end, const extern_type *&from_next,
                     intern_type *to, intern_type *to_end, intern_type *&to_next) const override
        {
            return utf8_to_ucs(reinterpret_cast<const char *>(from), reinterpret_cast<const char *>(from_end),
                               reinterpret_cast<const char *&>(from_next), to, to_end, to_next);
        }

        constexpr int do_encoding() const noexcept override { return 0; }
        constexpr bool do_always_noconv() const noexcept override { return false; }

        int do_length(state_type &, const extern_type *from, const extern_type *end, size_t max) const override
        {
            return utf8_count_length<char16_t>(reinterpret_cast<const char *>(from),
                                               reinterpret_cast<const char *>(end), max);
        }

        constexpr int do_max_length() const noexcept override { return 4; }
    };

    template<>
    class codecvt<char32_t, char8_t, mbstate_type> : public codecvt_abstract_base<char32_t, char8_t, mbstate_type>
    {
    public:
        typedef char32_t intern_type;
        typedef char8_t extern_type;
        typedef mbstate_type state_type;

    public:
        static locale::id id;

        explicit codecvt(size_t refs = 0) : codecvt_abstract_base<char32_t, char8_t, mbstate_type>(refs) {}

    protected:
        virtual ~codecvt() {}

        result do_out(state_type &, const intern_type *from, const intern_type *from_end, const intern_type *&from_next,
                      extern_type *to, extern_type *to_end, extern_type *&to_next) const override
        {
            return ucs_to_utf8(from, from_end, from_next, reinterpret_cast<char *>(to),
                               reinterpret_cast<char *>(to_end), reinterpret_cast<char *&>(to_next));
        }

        constexpr result do_unshift(state_type &, extern_type *to, extern_type *, extern_type *&to_next) const override
        {
            to_next = to;
            return noconv;
        }

        result do_in(state_type &, const extern_type *from, const extern_type *from_end, const extern_type *&from_next,
                     intern_type *to, intern_type *to_end, intern_type *&to_next) const override
        {
            return utf8_to_ucs(reinterpret_cast<const char *>(from), reinterpret_cast<const char *>(from_end),
                               reinterpret_cast<const char *&>(from_next), to, to_end, to_next);
        }

        constexpr int do_encoding() const noexcept override { return 0; }
        constexpr bool do_always_noconv() const noexcept override { return false; }

        int do_length(state_type &, const extern_type *from, const extern_type *end, size_t max) const override
        {
            return utf8_count_length<char32_t>(reinterpret_cast<const char *>(from),
                                               reinterpret_cast<const char *>(end), max);
        }

        constexpr int do_max_length() const noexcept override { return 4; }
    };

    template<typename Internal, typename External, typename State>
    class codecvt_byname : public codecvt<Internal, External, State>
    {
    public:
        explicit codecvt_byname(const char *s, size_t refs = 0) : codecvt<Internal, External, State>(refs)
        {
            if (builtin_strcmp(s, "C") != 0 && builtin_strcmp(s, "POSIX") != 0)
            {
                // NOTE: fixed from _S_destroy_c_locale/_S_create_c_locale,
                // which don't exist on locale::facet -- those are
                // destroy_c_locale/create_c_locale (no _S_ prefix).
                this->destroy_c_locale(this->_M_c_locale_codecvt);
                this->create_c_locale(this->_M_c_locale_codecvt, s);
            }
        }

        explicit codecvt_byname(const string &s, size_t refs = 0) : codecvt_byname(s.c_str(), refs) {}

    protected:
        virtual ~codecvt_byname() {}
    };

    template<>
    class codecvt_byname<char16_t, char, mbstate_type> : public codecvt<char16_t, char, mbstate_type>
    {
    public:
        explicit codecvt_byname(const char *, size_t refs = 0) : codecvt<char16_t, char, mbstate_type>(refs) {}

        explicit codecvt_byname(const string &s, size_t refs = 0) : codecvt_byname(s.c_str(), refs) {}

    protected:
        virtual ~codecvt_byname() {}
    };

    template<>
    class codecvt_byname<char32_t, char, mbstate_type> : public codecvt<char32_t, char, mbstate_type>
    {
    public:
        explicit codecvt_byname(const char *, size_t refs = 0) : codecvt<char32_t, char, mbstate_type>(refs) {}

        explicit codecvt_byname(const string &s, size_t refs = 0) : codecvt_byname(s.c_str(), refs) {}

    protected:
        virtual ~codecvt_byname() {}
    };

    template<>
    class codecvt_byname<char16_t, char8_t, mbstate_type> : public codecvt<char16_t, char8_t, mbstate_type>
    {
    public:
        explicit codecvt_byname(const char *, size_t refs = 0) : codecvt<char16_t, char8_t, mbstate_type>(refs) {}

        explicit codecvt_byname(const string &s, size_t refs = 0) : codecvt_byname(s.c_str(), refs) {}

    protected:
        virtual ~codecvt_byname() {}
    };

    template<>
    class codecvt_byname<char32_t, char8_t, mbstate_type> : public codecvt<char32_t, char8_t, mbstate_type>
    {
    public:
        explicit codecvt_byname(const char *, size_t refs = 0) : codecvt<char32_t, char8_t, mbstate_type>(refs) {}

        explicit codecvt_byname(const string &s, size_t refs = 0) : codecvt_byname(s.c_str(), refs) {}

    protected:
        virtual ~codecvt_byname() {}
    };
} // namespace SFTL
