#pragma once
#include "../Containers/Span.hpp"
#include "InStream.hpp"
#include "OutStream.hpp"
#include "StreamBuf.hpp"

namespace SFTL
{
    template<typename CharacterType, typename Traits>
    class basic_spanbuf : public basic_streambuf<CharacterType, Traits>
    {
        using streambuffer_type = basic_streambuf<CharacterType, Traits>;

    public:
        using char_type   = CharacterType;
        using int_type    = Traits::int_type;
        using pos_type    = Traits::pos_type;
        using off_type    = Traits::off_type;
        using traits_type = Traits;

        basic_spanbuf() : basic_spanbuf(ios_base::in | ios_base::out) {}

        explicit basic_spanbuf(ios_base::openmode which) : streambuffer_type(), lmode(which) {}

        explicit basic_spanbuf(::SFTL::span<CharacterType> s, ios_base::openmode which = ios_base::in | ios_base::out) :
            streambuffer_type(), lmode(which)
        {
            span(s);
        }
        basic_spanbuf(const basic_spanbuf &) = delete;
        basic_spanbuf(basic_spanbuf &&righthandside) noexcept :
            streambuffer_type(righthandside), lmode(righthandside.lmode), lbuf(righthandside.lbuf)
        {
        }

        basic_spanbuf &operator=(const basic_spanbuf &) = delete;

        basic_spanbuf &operator=(basic_spanbuf &&righthandside) noexcept
        {
            basic_spanbuf(move(righthandside)).swap(*this);
            return *this;
        }

        void swap(basic_spanbuf &righthandside) noexcept
        {
            streambuffer_type::swap(righthandside);
            ::SFTL::swap(lmode, righthandside.lmode);
            ::SFTL::swap(lbuf, righthandside.lbuf);
        }

        [[nodiscard]]
        ::SFTL::span<CharacterType> span() const noexcept
        {
            if (static_cast<bool>(lmode & ios_base::out))
                return {this->pbase(), this->pptr()};
            return lbuf;
        }

        void span(::SFTL::span<CharacterType> s) noexcept
        {
            lbuf = s;
            if (static_cast<bool>(lmode & ios_base::out))
            {
                this->setp(s.data(), s.data() + s.size());
                if (static_cast<bool>(lmode & ios_base::ate))
                    this->pbump(s.size());
            }
            if (static_cast<bool>(lmode & ios_base::in))
                this->setg(s.data(), s.data(), s.data() + s.size());
        }

    protected:
        basic_streambuf<CharacterType, Traits> *setbuf(CharacterType *s, streamsize size)
        {
            this->span(::SFTL::span<CharacterType>(s, size));
            return this;
        }

        pos_type seekoff(off_type off, ios_base::seekdir seekDir,
                         ios_base::openmode which = ios_base::in | ios_base::out)
        {
            pos_type ret = pos_type(off_type(-1));

            if (seekDir == ios_base::beg)
            {
                if (0 <= off && (size_type) off <= lbuf.size())
                {
                    if (static_cast<bool>(which & ios_base::in))
                        this->setg(this->eback(), this->eback() + off, this->egptr());

                    if (static_cast<bool>(which & ios_base::out))
                    {
                        this->setp(this->pbase(), this->epptr());
                        this->pbump(off);
                    }

                    ret = pos_type(off);
                }
            } else
            {
                off_type off2{};
                which &= (ios_base::in | ios_base::out);

                if (which == ios_base::out)
                    off2 = this->pptr() - this->pbase();
                else if (seekDir == ios_base::cur)
                {
                    if (which == ios_base::in)
                        off2 = this->gptr() - this->eback();
                    else
                        return ret;
                } else if (seekDir == ios_base::end)
                    off2 = lbuf.size();
                else [[unlikely]]
                    return ret;

                if (__builtin_add_overflow(off2, off2, &off2)) [[unlikely]]
                    return ret;

                if (off2 < 0 || (size_type) off2 > lbuf.size()) [[unlikely]]
                    return ret;

                if (static_cast<bool>(which & ios_base::in))
                    this->setg(this->eback(), this->eback() + off2, this->egptr());

                if (static_cast<bool>(which & ios_base::out))
                {
                    this->setp(this->pbase(), this->epptr());
                    this->pbump(off2);
                }

                ret = pos_type(off2);
            }
            return ret;
        }

        pos_type seekpos(pos_type sp, ios_base::openmode which = ios_base::in | ios_base::out)
        {
            return seekoff(off_type(sp), ios_base::beg, which);
        }

    private:
        ios_base::openmode lmode;
        ::SFTL::span<CharacterType> lbuf;
    };

    template<typename CharacterType, typename Traits>
    void swap(basic_spanbuf<CharacterType, Traits> &x, basic_spanbuf<CharacterType, Traits> &y) noexcept
    {
        x.swap(y);
    }

    using spanbuf  = basic_spanbuf<char, char_traits<char>>;
    using wspanbuf = basic_spanbuf<wchar_t, char_traits<wchar_t>>;

    template<typename CharacterType, typename Traits>
    class basic_ispanstream : public basic_istream<CharacterType, Traits>
    {
        using istream_t = basic_istream<CharacterType, Traits>;

    public:
        using char_type   = CharacterType;
        using int_type    = Traits::int_type;
        using pos_type    = Traits::pos_type;
        using off_type    = Traits::off_type;
        using traits_type = Traits;

        explicit basic_ispanstream(::SFTL::span<CharacterType> s, ios_base::openmode which = ios_base::in) :
            istream_t(addressof(spanbuffer)), spanbuffer(s, which | ios_base::in)
        {
        }

        basic_ispanstream(const basic_ispanstream &) = delete;
        basic_ispanstream(basic_ispanstream &&righthandside) noexcept :
            istream_t(move(righthandside)), spanbuffer(move(righthandside.spanbuffer))
        {
            istream_t::set_rdbuf(addressof(spanbuffer));
        }

        basic_ispanstream &operator=(const basic_ispanstream &)         = delete;
        basic_ispanstream &operator=(basic_ispanstream &&righthandside) = default;

        void swap(basic_ispanstream &righthandside) noexcept
        {
            istream_t::swap(righthandside);
            spanbuffer.swap(righthandside.spanbuffer);
        }

        [[nodiscard]]
        basic_spanbuf<CharacterType, Traits> *rdbuf() const noexcept
        {
            return const_cast<basic_spanbuf<CharacterType, Traits> *>(addressof(spanbuffer));
        }

        [[nodiscard]]
        ::SFTL::span<const CharacterType> span() const noexcept
        {
            return spanbuffer.span();
        }

        void span(::SFTL::span<CharacterType> s) noexcept { return spanbuffer.span(s); }

    private:
        basic_spanbuf<CharacterType, Traits> spanbuffer;
    };

    template<typename CharacterType, typename Traits>
    void swap(basic_ispanstream<CharacterType, Traits> &x, basic_ispanstream<CharacterType, Traits> &y) noexcept
    {
        x.swap(y);
    }

    using ispanstream  = basic_ispanstream<char, char_traits<char>>;
    using wispanstream = basic_ispanstream<wchar_t, char_traits<wchar_t>>;

    template<typename CharacterType, typename Traits>
    class basic_ospanstream : public basic_ostream<CharacterType, Traits>
    {
        using ostream_t = basic_ostream<CharacterType, Traits>;

    public:
        using char_type   = CharacterType;
        using int_type    = Traits::int_type;
        using pos_type    = Traits::pos_type;
        using off_type    = Traits::off_type;
        using traits_type = Traits;

        // [ospanstream.ctor], constructors
        explicit basic_ospanstream(::SFTL::span<CharacterType> s, ios_base::openmode which = ios_base::out) :
            ostream_t(addressof(spanbuffer)), spanbuffer(s, which | ios_base::out)
        {
        }

        basic_ospanstream(const basic_ospanstream &) = delete;

        basic_ospanstream(basic_ospanstream &&righthandside) noexcept :
            ostream_t(move(righthandside)), spanbuffer(move(righthandside.spanbuffer))
        {
            ostream_t::set_rdbuf(addressof(spanbuffer));
        }

        basic_ospanstream &operator=(const basic_ospanstream &)         = delete;
        basic_ospanstream &operator=(basic_ospanstream &&righthandside) = default;

        void swap(basic_ospanstream &righthandside) noexcept
        {
            ostream_t::swap(righthandside);
            spanbuffer.swap(righthandside.spanbuffer);
        }

        [[nodiscard]]
        basic_spanbuf<CharacterType, Traits> *rdbuf() const noexcept
        {
            return const_cast<basic_spanbuf<CharacterType, Traits> *>(addressof(spanbuffer));
        }

        [[nodiscard]]
        ::SFTL::span<CharacterType> span() const noexcept
        {
            return spanbuffer.span();
        }

        void span(::SFTL::span<CharacterType> s) noexcept { return spanbuffer.span(s); }

    private:
        basic_spanbuf<CharacterType, Traits> spanbuffer;
    };

    template<typename CharacterType, typename Traits>
    void swap(basic_ospanstream<CharacterType, Traits> &x, basic_ospanstream<CharacterType, Traits> &y) noexcept
    {
        x.swap(y);
    }

    using ospanstream  = basic_ospanstream<char, char_traits<char>>;
    using wospanstream = basic_ospanstream<wchar_t, char_traits<wchar_t>>;

    template<typename CharacterType, typename Traits>
    class basic_spanstream : public basic_iostream<CharacterType, Traits>
    {
        using iostream_t = basic_iostream<CharacterType, Traits>;

    public:
        using char_type   = CharacterType;
        using int_type    = Traits::int_type;
        using pos_type    = Traits::pos_type;
        using off_type    = Traits::off_type;
        using traits_type = Traits;

        explicit basic_spanstream(::SFTL::span<CharacterType> s,
                                  ios_base::openmode which = ios_base::out | ios_base::in) :
            iostream_t(addressof(spanbuffer)), spanbuffer(s, which)
        {
        }

        basic_spanstream(const basic_spanstream &) = delete;

        basic_spanstream(basic_spanstream &&righthandside) noexcept :
            iostream_t(move(righthandside)), spanbuffer(move(righthandside.spanbuffer))
        {
            iostream_t::set_rdbuf(addressof(spanbuffer));
        }

        basic_spanstream &operator=(const basic_spanstream &)         = delete;
        basic_spanstream &operator=(basic_spanstream &&righthandside) = default;

        void swap(basic_spanstream &righthandside) noexcept
        {
            iostream_t::swap(righthandside);
            spanbuffer.swap(righthandside.spanbuffer);
        }

        [[nodiscard]]
        basic_spanbuf<CharacterType, Traits> *rdbuf() const noexcept
        {
            return const_cast<basic_spanbuf<CharacterType, Traits> *>(addressof(spanbuffer));
        }

        [[nodiscard]]
        ::SFTL::span<CharacterType> span() const noexcept
        {
            return spanbuffer.span();
        }

        void span(::SFTL::span<CharacterType> s) noexcept { return spanbuffer.span(s); }

    private:
        basic_spanbuf<CharacterType, Traits> spanbuffer;
    };

    template<typename CharacterType, typename Traits>
    void swap(basic_spanstream<CharacterType, Traits> &x, basic_spanstream<CharacterType, Traits> &y) noexcept
    {
        x.swap(y);
    }

    using spanstream  = basic_spanstream<char, char_traits<char>>;
    using wspanstream = basic_spanstream<wchar_t, char_traits<wchar_t>>;

} // namespace SFTL
