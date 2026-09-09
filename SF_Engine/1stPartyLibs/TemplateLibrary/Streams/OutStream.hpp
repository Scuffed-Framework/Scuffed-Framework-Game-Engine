#pragma once

#include "../CType.hpp"
#include "IOS.hpp"

namespace SFTL
{
    template<typename CharacterType, typename Traits>
    class basic_ostream : virtual public basic_ios<CharacterType, Traits>
    {
    public:
        typedef CharacterType char_type;
        typedef Traits::int_type int_type;
        typedef Traits::pos_type pos_type;
        typedef Traits::off_type off_type;
        typedef Traits traits_type;

        typedef basic_streambuf<CharacterType, Traits> streambuf_type;
        typedef basic_ios<CharacterType, Traits> ios_type;
        typedef basic_ostream ostream_type;
        typedef num_put<CharacterType, ostreambuf_iterator<CharacterType, Traits>> num_put_type;
        typedef ctype<CharacterType> ctype_type;

        explicit basic_ostream(streambuf_type *sb) { this->init(sb); }
        virtual ~basic_ostream() {}
        class sentry;
        friend class sentry;

        ostream_type &operator<<(ostream_type &(*pf)(ostream_type &) ) { return pf(*this); }
        ostream_type &operator<<(ios_type &(*pf)(ios_type &) )
        {
            pf(*this);
            return *this;
        }
        ostream_type &operator<<(ios_base &(*pf)(ios_base &) )
        {
            pf(*this);
            return *this;
        }

        ostream_type &operator<<(bool num) { return Linsert(num); }
        ostream_type &operator<<(short num);
        ostream_type &operator<<(unsigned short num) { return Linsert(static_cast<unsigned long>(num)); }
        ostream_type &operator<<(int num);
        ostream_type &operator<<(unsigned int num) { return Linsert(static_cast<unsigned long>(num)); }
        ostream_type &operator<<(long num) { return Linsert(num); }
        ostream_type &operator<<(unsigned long num) { return Linsert(num); }
        ostream_type &operator<<(long long num) { return Linsert(num); }
        ostream_type &operator<<(unsigned long long num) { return Linsert(num); }
        ostream_type &operator<<(float num) { return Linsert(static_cast<double>(num)); }
        ostream_type &operator<<(double num) { return Linsert(num); }
        ostream_type &operator<<(long double num) { return Linsert(num); }
        ostream_type &operator<<(const void *ptr) { return Linsert(ptr); }
        ostream_type &operator<<(streambuf_type *sb);

        ostream_type &put(char_type character);
        ostream_type &write(const char_type *arr, streamsize num);

        ostream_type &flush();

        pos_type tellp();
        ostream_type &seekp(pos_type);
        ostream_type &seekp(off_type, ios_base::seekdir);

    protected:
        basic_ostream() { this->init(0); }
        basic_ostream(const basic_ostream &) = delete;
        basic_ostream(basic_ostream &&rhs) noexcept : ios_type() { ios_type::move(rhs); }

        basic_ostream &operator=(const basic_ostream &) = delete;
        basic_ostream &operator=(basic_ostream &&rhs) noexcept
        {
            swap(rhs);
            return *this;
        }

        void swap(basic_ostream &rhs) noexcept { ios_type::swap(rhs); }

        template<typename ValueType>
        ostream_type &Linsert(ValueType v);
    };

    template<typename CharacterType, typename Traits>
    class basic_ostream<CharacterType, Traits>::sentry
    {
        bool ok;
        basic_ostream<CharacterType, Traits> &os;

    public:
        typedef Traits traits_type;
        typedef basic_streambuf<CharacterType, Traits> streambuf_type;
        typedef basic_ostream<CharacterType, Traits> ostream_type;
        typedef typename ostream_type::ctype_type ctype_type;

        explicit sentry(basic_ostream<CharacterType, Traits> &os);
        ~sentry();

        sentry(const sentry &)            = delete;
        sentry &operator=(const sentry &) = delete;

        explicit operator bool() const { return ok; }
    };

    template<typename CharacterType, typename Traits>
    basic_ostream<CharacterType, Traits> &operator<<(basic_ostream<CharacterType, Traits> &out,
                                                     CharacterType character);

    template<class Traits>
    inline basic_ostream<char, Traits> &operator<<(basic_ostream<char, Traits> &out, unsigned char character)
    {
        return (out << static_cast<char>(character));
    }

    template<class Traits>
    inline basic_ostream<char, Traits> &operator<<(basic_ostream<char, Traits> &out, signed char character)
    {
        return (out << static_cast<char>(character));
    }

    template<typename CharacterType, typename Traits>
    basic_ostream<CharacterType, Traits> &operator<<(basic_ostream<CharacterType, Traits> &out, const CharacterType *s);

    template<class Traits>
    inline basic_ostream<char, Traits> &operator<<(basic_ostream<char, Traits> &out, const unsigned char *s)
    {
        return (out << reinterpret_cast<const char *>(s));
    }

    template<class Traits>
    inline basic_ostream<char, Traits> &operator<<(basic_ostream<char, Traits> &out, const signed char *s)
    {
        return (out << reinterpret_cast<const char *>(s));
    }

    // Manipulators.
    template<typename CharacterType, typename Traits>
    inline basic_ostream<CharacterType, Traits> &endl(basic_ostream<CharacterType, Traits> &os)
    {
        os.put(os.widen('\n'));
        os.flush();
        return os;
    }

    template<typename CharacterType, typename Traits>
    inline basic_ostream<CharacterType, Traits> &ends(basic_ostream<CharacterType, Traits> &os)
    {
        os.put(CharacterType());
        return os;
    }

    template<typename CharacterType, typename Traits>
    inline basic_ostream<CharacterType, Traits> &flush(basic_ostream<CharacterType, Traits> &os)
    {
        return os.flush();
    }
} // namespace SFTL
