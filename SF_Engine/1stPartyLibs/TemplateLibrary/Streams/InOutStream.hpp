#pragma once

#include "../CType.hpp"
#include "IOS.hpp"
#include "InStream.hpp"
#include "OutStream.hpp"

namespace SFTL
{
    template<typename CharacterType, typename Traits>
    class basic_iostream : public basic_istream<CharacterType, Traits>, public basic_ostream<CharacterType, Traits>
    {
    public:
        typedef CharacterType char_type;
        typedef Traits::int_type int_type;
        typedef Traits::pos_type pos_type;
        typedef Traits::off_type off_type;
        typedef Traits traits_type;

        typedef basic_streambuf<CharacterType, Traits> streambuf_type;
        typedef basic_istream<CharacterType, Traits> istream_type;
        typedef basic_ostream<CharacterType, Traits> ostream_type;

        explicit basic_iostream(streambuf_type *sb) : istream_type(sb), ostream_type(sb) {}

        ~basic_iostream() override {}

    protected:
        basic_iostream() : istream_type(), ostream_type() {}
        basic_iostream(const basic_iostream &) = delete;

        basic_iostream(basic_iostream &&righthandside) noexcept :
            istream_type(move(righthandside)), ostream_type(move(righthandside))
        {
        }

        basic_iostream &operator=(const basic_iostream &) = delete;
        basic_iostream &operator=(basic_iostream &&righthandside) noexcept
        {
            swap(righthandside);
            return *this;
        }

        void swap(basic_iostream &righthandside) noexcept { istream_type::swap(righthandside); }
    };

    using iostream  = basic_iostream<char, char_traits<char>>;
    using wiostream = basic_iostream<wchar_t, char_traits<wchar_t>>;

    /**
     * @breif << character to standard out
     */
    extern ostream cout;
    /**
     * @breif << character to buffered standard error
     */
    extern ostream cerr;
    /**
     * @breif << character to standard error
     */
    extern ostream clog;
    /**
     * @breif >> character from standard in
     */
    extern istream cin;

    /**
     * @breif << character to standard out
     */
    extern wostream wcout;
    /**
     * @breif << character to buffered standard error
     */
    extern wostream wcerr;
    /**
     * @breif << character to standard error
     */
    extern wostream wclog;
    /**
     * @breif >> character from standard in
     */
    extern wistream wcin;

} // namespace SFTL
