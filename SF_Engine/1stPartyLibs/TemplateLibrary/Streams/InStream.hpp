#pragma once

#include "../CType.hpp"
#include "../NumericProperties.hpp"
#include "StreamBuf.hpp"

namespace SFTL
{
    template<typename CharacterType, typename Traits>
    class basic_istream : virtual public basic_ios<CharacterType, Traits>
    {
    public:
        typedef CharacterType char_type;
        typedef Traits::int_type int_type;
        typedef Traits::pos_type pos_type;
        typedef Traits::off_type off_type;
        typedef Traits traits_type;

        typedef basic_streambuf<CharacterType, Traits> streambuf_type;
        typedef basic_ios<CharacterType, Traits> ios_type;
        typedef basic_istream<CharacterType, Traits> istream_type;
        typedef num_get<CharacterType, istreambuf_iterator<CharacterType, Traits>> num_get_type;
        typedef ctype<CharacterType> ctype_type;

    protected:
        streamsize lgcount;

    public:
        explicit basic_istream(streambuf_type *sb) : lgcount(streamsize(0)) { this->init(sb); }
        virtual ~basic_istream() { lgcount = streamsize(0); }
        class sentry;
        friend class sentry;

        istream_type &operator>>(istream_type &(*pf)(istream_type &) ) { return pf(*this); }
        istream_type &operator>>(ios_type &(*pf)(ios_type &) )
        {
            pf(*this);
            return *this;
        }
        istream_type &operator>>(ios_base &(*pf)(ios_base &) )
        {
            pf(*this);
            return *this;
        }
        istream_type &operator>>(bool &num) { return Lextract(num); }
        istream_type &operator>>(short &num);
        istream_type &operator>>(unsigned short &num) { return Lextract(num); }
        istream_type &operator>>(int &num);
        istream_type &operator>>(unsigned int &num) { return Lextract(num); }
        istream_type &operator>>(long &num) { return Lextract(num); }
        istream_type &operator>>(unsigned long &num) { return Lextract(num); }
        istream_type &operator>>(long long &num) { return Lextract(num); }
        istream_type &operator>>(unsigned long long &num) { return Lextract(num); }
        istream_type &operator>>(float &f) { return Lextract(f); }
        istream_type &operator>>(double &f) { return Lextract(f); }
        istream_type &operator>>(long double &f) { return Lextract(f); }
        istream_type &operator>>(void *&p) { return Lextract(p); }
        istream_type &operator>>(streambuf_type *sb);

        [[nodiscard]] streamsize gcount() const { return lgcount; }
        int_type get();
        istream_type &get(char_type &character);
        istream_type &get(char_type *arr, streamsize num, char_type delim);
        istream_type &get(char_type *arr, streamsize num) { return this->get(arr, num, this->widen('\n')); }
        istream_type &get(streambuf_type &sb, char_type delim);
        istream_type &get(streambuf_type &sb) { return this->get(sb, this->widen('\n')); }

        istream_type &getline(char_type *arr, streamsize num, char_type delim);
        istream_type &getline(char_type *arr, streamsize num) { return this->getline(arr, num, this->widen('\n')); }


        istream_type &ignore(streamsize num, int_type delim);
        istream_type &ignore(streamsize num);
        istream_type &ignore();

        int_type peek();


        istream_type &read(char_type *arr, streamsize num);

        streamsize readsome(char_type *arr, streamsize num);
        istream_type &putback(char_type character);


        istream_type &unget();

        int sync();

        pos_type tellg();
        istream_type &seekg(pos_type);
        istream_type &seekg(off_type, ios_base::seekdir);

    protected:
        basic_istream() : lgcount(streamsize(0)) { this->init(0); }
        basic_istream(const basic_istream &) = delete;
        basic_istream(basic_istream &&rhs) noexcept : ios_type(), lgcount(rhs.lgcount)
        {
            ios_type::move(rhs);
            rhs.lgcount = 0;
        }

        basic_istream &operator=(const basic_istream &) = delete;
        basic_istream &operator=(basic_istream &&rhs) noexcept
        {
            swap(rhs);
            return *this;
        }

        void swap(basic_istream &rhs) noexcept
        {
            ios_type::swap(rhs);
            ::SFTL::swap(lgcount, rhs.lgcount);
        }

        template<typename ValueType>
        istream_type &Lextract(ValueType &v);
    };

    template<>
    basic_istream<char> &basic_istream<char>::getline(char_type *arr, streamsize num, char_type delim);
    template<>
    basic_istream<char> &basic_istream<char>::ignore(streamsize num);
    template<>
    basic_istream<char> &basic_istream<char>::ignore(streamsize num, int_type delim);
    template<>
    basic_istream<wchar_t> &basic_istream<wchar_t>::getline(char_type *arr, streamsize num, char_type delim);
    template<>
    basic_istream<wchar_t> &basic_istream<wchar_t>::ignore(streamsize num);
    template<>
    basic_istream<wchar_t> &basic_istream<wchar_t>::ignore(streamsize num, int_type delim);

    template<typename CharacterType, typename Traits>
    class basic_istream<CharacterType, Traits>::sentry
    {
        bool ok;

    public:
        typedef Traits traits_type;
        typedef basic_streambuf<CharacterType, Traits> streambuf_type;
        typedef basic_istream<CharacterType, Traits> istream_type;
        typedef typename istream_type::ctype_type ctype_type;
        typedef typename Traits::int_type int_type;

        explicit sentry(basic_istream<CharacterType, Traits> &is, bool noskipws = false);

        explicit operator bool() const { return ok; }
    };


    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &operator>>(basic_istream<CharacterType, Traits> &in,
                                                     CharacterType &character);

    template<class Traits>
    inline basic_istream<char, Traits> &operator>>(basic_istream<char, Traits> &in, unsigned char &character)
    {
        return (in >> reinterpret_cast<char &>(character));
    }

    template<class Traits>
    inline basic_istream<char, Traits> &operator>>(basic_istream<char, Traits> &in, signed char &character)
    {
        return (in >> reinterpret_cast<char &>(character));
    }

    template<typename CharacterType, typename Traits>
    void istream_extract(basic_istream<CharacterType, Traits> &, CharacterType *, streamsize);
    void istream_extract(istream &, char *, streamsize);

    template<typename CharacterType, typename Traits, size_t Number>
    inline basic_istream<CharacterType, Traits> &operator>>(basic_istream<CharacterType, Traits> &in,
                                                            CharacterType (&arr)[Number])
    {
        static_assert(Number <= ::SFTL::Detail::__numeric_traits<streamsize>::__max);
        istream_extract(in, arr, Number);
        return in;
    }

    template<class Traits, size_t Number>
    inline basic_istream<char, Traits> &operator>>(basic_istream<char, Traits> &in, unsigned char (&arr)[Number])
    {
        return in >> reinterpret_cast<char (&)[Number]>(arr);
    }

    template<class Traits, size_t Number>
    inline basic_istream<char, Traits> &operator>>(basic_istream<char, Traits> &in, signed char (&arr)[Number])
    {
        return in >> reinterpret_cast<char (&)[Number]>(arr);
    }


    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &ws(basic_istream<CharacterType, Traits> &is);

    template<typename T, typename Type>
        requires derived_from_ios_base<T> && requires(T &is, Type &&t) { is >> forward<Type>(t); }
    using rvalue_stream_extraction_t = T &&;

    template<typename IStream, typename Type>
    rvalue_stream_extraction_t<IStream, Type> operator>>(IStream &&is, Type &&x)
    {
        is >> forward<Type>(x);
        return move(is);
    }

} // namespace SFTL
