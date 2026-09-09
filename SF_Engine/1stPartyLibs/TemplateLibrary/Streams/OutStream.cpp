/******************************************************************************/
/* OutStream.cpp                                                              */
/******************************************************************************/
/*            This file is part of                                            */
/*            Scuffed Framework Standard Template Library                     */
/******************************************************************************/
#include "OutStream.hpp"

namespace SFTL
{
    template<typename Character, typename Traits>
    basic_ostream<Character, Traits>::sentry::sentry(basic_ostream<Character, Traits> &out) : ok(false), os(out)
    {
        if (out.good())
        {
            if (out.tie())
                out.tie()->flush();
        }
        ok = out.good();
        if (!ok)
            out.setstate(ios_base::failbit);
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits>::sentry::~sentry()
    {
        // NOTE: a real implementation should also check that no exception is
        // currently propagating (std::uncaught_exceptions()) before letting a
        // failed sync raise badbit here; simplified for this port.
        if (bool(os.flags() & ios_base::unitbuf) && !os.fail())
        {
            if (streambuf_type *sb = os.rdbuf(); sb && sb->pubsync() == -1)
                os.setstate(ios_base::badbit);
        }
    }

    template<typename Character, typename Traits>
    template<typename Value>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::Linsert(Value v)
    {
        sentry sy(*this);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            const num_put_type &np  = check_facet(this->lnum_put);
            if (np.put(*this, *this, this->fill(), v).failed())
                error |= ios_base::badbit;
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::operator<<(short n)
    {
        const ios_base::fmtflags base = this->flags() & ios_base::basefield;
        if (base == ios_base::oct || base == ios_base::hex)
            return Linsert(static_cast<long>(static_cast<unsigned short>(n)));
        else
            return Linsert(static_cast<long>(n));
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::operator<<(int n)
    {
        if (const ios_base::fmtflags base = this->flags() & ios_base::basefield;
            base == ios_base::oct || base == ios_base::hex)
            return Linsert(static_cast<long>(static_cast<unsigned int>(n)));
        else
            return Linsert(static_cast<long>(n));
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::operator<<(streambuf_type *sbin)
    {
        ios_base::iostate error = ios_base::goodbit;
        sentry sy(*this);
        if (sy && sbin)
        {
            if (streambuf_type *sbout = this->rdbuf())
            {
                const int_type eof = traits_type::eof();
                bool wrote_any     = false;
                int_type c         = sbin->sgetc();
                while (!traits_type::eq_int_type(c, eof))
                {
                    if (traits_type::eq_int_type(sbout->sputc(traits_type::to_char_type(c)), eof))
                    {
                        error |= ios_base::badbit;
                        break;
                    }
                    wrote_any = true;
                    c         = sbin->snextc();
                }
                if (!wrote_any)
                    error |= ios_base::failbit;
            } else
            {
                error |= ios_base::badbit;
            }
        } else if (!sbin)
        {
            error |= ios_base::failbit;
        }
        if (static_cast<bool>(error))
            this->setstate(error);
        return *this;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::put(char_type c)
    {
        sentry sy(*this);
        bool failed = true;
        if (sy)
        {
            if (streambuf_type *sb = this->rdbuf())
                failed = traits_type::eq_int_type(sb->sputc(c), traits_type::eof());
        }
        if (failed)
            this->setstate(ios_base::badbit);
        return *this;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::write(const char_type *s, streamsize n)
    {
        sentry sy(*this);
        if (sy)
        {
            if (streambuf_type *sb = this->rdbuf())
            {
                if (n > 0 && sb->sputn(s, n) != n)
                    this->setstate(ios_base::badbit);
            } else
            {
                this->setstate(ios_base::badbit);
            }
        }
        return *this;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::flush()
    {
        ios_base::iostate error = ios_base::goodbit;
        if (streambuf_type *sb = this->rdbuf())
        {
            if (this->good() && sb->pubsync() == -1)
                error |= ios_base::badbit;
        }
        if (static_cast<bool>(error))
            this->setstate(error);
        return *this;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits>::pos_type basic_ostream<Character, Traits>::tellp()
    {
        pos_type ret = pos_type(-1);
        if (!this->fail())
        {
            if (streambuf_type *sb = this->rdbuf())
                ret = sb->pubseekoff(0, ios_base::cur, ios_base::out);
        }
        return ret;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::seekp(pos_type pos)
    {
        if (!this->fail())
        {
            if (streambuf_type *sb = this->rdbuf())
            {
                if (sb->pubseekpos(pos, ios_base::out) == pos_type(off_type(-1)))
                    this->setstate(ios_base::failbit);
            } else
            {
                this->setstate(ios_base::failbit);
            }
        }
        return *this;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &basic_ostream<Character, Traits>::seekp(off_type off, ios_base::seekdir dir)
    {
        if (!this->fail())
        {
            if (streambuf_type *sb = this->rdbuf())
            {
                if (sb->pubseekoff(off, dir, ios_base::out) == pos_type(off_type(-1)))
                    this->setstate(ios_base::failbit);
            } else
            {
                this->setstate(ios_base::failbit);
            }
        }
        return *this;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &operator<<(basic_ostream<Character, Traits> &out, Character c)
    {
        typedef basic_ostream<Character, Traits> ostream_type;

        typename ostream_type::sentry sy(out);
        if (sy)
        {
            const streamsize w = out.width();
            const bool left    = w > 0 && (out.flags() & ios_base::adjustfield) == ios_base::left;

            if (w > 1 && !left)
                for (streamsize i = 1; i < w; ++i)
                    out.put(out.fill());

            out.put(c);

            if (w > 1 && left)
                for (streamsize i = 1; i < w; ++i)
                    out.put(out.fill());

            out.width(0);
        }
        return out;
    }

    template<typename Character, typename Traits>
    basic_ostream<Character, Traits> &operator<<(basic_ostream<Character, Traits> &out, const Character *s)
    {
        typedef basic_ostream<Character, Traits> ostream_type;
        typedef typename ostream_type::streambuf_type streambuf_type;

        if (!s)
        {
            out.setstate(ios_base::badbit);
            return out;
        }

        typename ostream_type::sentry sy(out);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            const auto n            = static_cast<streamsize>(Traits::length(s));
            const streamsize w      = out.width();
            const bool left         = w > n && (out.flags() & ios_base::adjustfield) == ios_base::left;

            if (w > n && !left)
                for (streamsize i = n; i < w; ++i)
                    out.put(out.fill());

            if (streambuf_type *sb = out.rdbuf())
            {
                if (n > 0 && sb->sputn(s, n) != n)
                    error |= ios_base::badbit;
            } else
            {
                error |= ios_base::badbit;
            }

            if (w > n && left)
                for (streamsize i = n; i < w; ++i)
                    out.put(out.fill());

            out.width(0);
            if (static_cast<bool>(error))
                out.setstate(error);
        }
        return out;
    }
} // namespace SFTL
