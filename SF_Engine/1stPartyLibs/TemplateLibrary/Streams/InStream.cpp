/******************************************************************************/
/* InStream.cpp                                                               */
/******************************************************************************/
/*            This file is part of                                            */
/*            Scuffed Framework Standard Template Library                     */
/******************************************************************************/
#include "InStream.hpp"
#include "OutStream.hpp"

namespace SFTL
{
    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits>::sentry::sentry(basic_istream<CharacterType, Traits> &in, bool noskip) :
        ok(false)
    {
        ios_base::iostate error = ios_base::goodbit;
        if (static_cast<bool>(in.good()))
        {
            if (in.tie())
                in.tie()->flush();
            if (!noskip && bool(in.flags() & ios_base::skipws))
            {
                const int_type eof = traits_type::eof();
                if (streambuf_type *sb = in.rdbuf())
                {
                    int_type c           = sb->sgetc();
                    const ctype_type &ct = check_facet(in.get_ctype());
                    while (!traits_type::eq_int_type(c, eof) && ct.is(ctype_base::space, traits_type::to_char_type(c)))
                    {
                        c = sb->snextc();
                    }

                    if (traits_type::eq_int_type(c, eof))
                        error |= ios_base::eofbit;
                } else
                {
                    error |= ios_base::badbit;
                }
            }
        }

        if (in.good() && error == ios_base::goodbit)
        {
            ok = true;
        } else
        {
            error |= ios_base::failbit;
            in.setstate(error);
        }
    }

    template<typename CharacterType, typename Traits>
    template<typename ValueType>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::Lextract(ValueType &v)
    {
        sentry sy(*this, false);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            const num_get_type &ng  = check_facet(this->lnum_get);
            ng.get(*this, 0, *this, error, v);
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::operator>>(short &n)
    {
        sentry sy(*this, false);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            long l;
            const num_get_type &ng = check_facet(this->lnum_get);
            ng.get(*this, 0, *this, error, l);

            if (l < Detail::__numeric_traits<short>::__min)
            {
                error |= ios_base::failbit;
                n = Detail::__numeric_traits<short>::__min;
            } else if (l > Detail::__numeric_traits<short>::__max)
            {
                error |= ios_base::failbit;
                n = Detail::__numeric_traits<short>::__max;
            } else
            {
                n = static_cast<short>(l);
            }

            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::operator>>(int &n)
    {
        sentry sy(*this, false);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            long l;
            const num_get_type &ng = check_facet(this->lnum_get);
            ng.get(*this, 0, *this, error, l);

            if (l < Detail::__numeric_traits<int>::__min)
            {
                error |= ios_base::failbit;
                n = Detail::__numeric_traits<int>::__min;
            } else if (l > Detail::__numeric_traits<int>::__max)
            {
                error |= ios_base::failbit;
                n = Detail::__numeric_traits<int>::__max;
            } else
            {
                n = static_cast<int>(l);
            }

            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::operator>>(streambuf_type *sbout)
    {
        ios_base::iostate error = ios_base::goodbit;
        sentry sy(*this, false);
        if (sy && sbout)
        {
            if (streambuf_type *sbin = this->rdbuf())
            {
                const int_type eof = traits_type::eof();
                int_type c         = sbin->sgetc();
                bool hit_eof       = false;
                while (!traits_type::eq_int_type(c, eof))
                {
                    if (traits_type::eq_int_type(sbout->sputc(traits_type::to_char_type(c)), eof))
                    {
                        error |= ios_base::failbit;
                        break;
                    }
                    c = sbin->snextc();
                }
                if (traits_type::eq_int_type(c, eof))
                    hit_eof = true;
                if (hit_eof)
                    error |= ios_base::eofbit;
            } else
            {
                error |= ios_base::badbit;
            }
        } else if (!sbout)
        {
            error |= ios_base::failbit;
        }
        if (static_cast<bool>(error))
            this->setstate(error);
        return *this;
    }

    template<typename CharacterType, typename Traits>
    typename basic_istream<CharacterType, Traits>::int_type basic_istream<CharacterType, Traits>::get(void)
    {
        const int_type eof      = traits_type::eof();
        int_type c              = eof;
        lgcount                 = 0;
        ios_base::iostate error = ios_base::goodbit;
        sentry sy(*this, true);
        if (sy)
        {
            if (streambuf_type *sb = this->rdbuf())
            {
                c = sb->sbumpc();
                if (!traits_type::eq_int_type(c, eof))
                    lgcount = 1;
                else
                    error |= ios_base::eofbit;
            } else
            {
                error |= ios_base::badbit;
            }
        }
        if (!lgcount)
            error |= ios_base::failbit;
        if (static_cast<bool>(error))
            this->setstate(error);
        return c;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::get(char_type &c)
    {
        lgcount                 = 0;
        ios_base::iostate error = ios_base::goodbit;
        sentry sy(*this, true);
        if (sy)
        {
            if (streambuf_type *sb = this->rdbuf())
            {
                if (const int_type cb = sb->sbumpc(); !traits_type::eq_int_type(cb, traits_type::eof()))
                {
                    lgcount = 1;
                    c       = traits_type::to_char_type(cb);
                } else
                {
                    error |= ios_base::eofbit;
                }
            } else
            {
                error |= ios_base::badbit;
            }
        }
        if (!lgcount)
            error |= ios_base::failbit;
        if (static_cast<bool>(error))
            this->setstate(error);
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::get(char_type *s, streamsize n,
                                                                                    char_type delim)
    {
        lgcount                 = 0;
        ios_base::iostate error = ios_base::goodbit;
        sentry sy(*this, true);
        if (sy && n > 0)
        {
            const int_type idelim = traits_type::to_int_type(delim);
            const int_type eof    = traits_type::eof();
            if (streambuf_type *sb = this->rdbuf())
            {
                int_type c = sb->sgetc();
                while (lgcount + 1 < n && !traits_type::eq_int_type(c, eof) && !traits_type::eq_int_type(c, idelim))
                {
                    *s++ = traits_type::to_char_type(c);
                    ++lgcount;
                    c = sb->snextc();
                }
                if (traits_type::eq_int_type(c, eof))
                    error |= ios_base::eofbit;
            } else
            {
                error |= ios_base::badbit;
            }
        }
        if (n > 0)
            *s = char_type();
        if (!lgcount)
            error |= ios_base::failbit;
        if (static_cast<bool>(error))
            this->setstate(error);
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::get(streambuf_type &sb, char_type delim)
    {
        lgcount                 = 0;
        ios_base::iostate error = ios_base::goodbit;
        sentry sy(*this, true);
        if (sy)
        {
            const int_type idelim = traits_type::to_int_type(delim);
            const int_type eof    = traits_type::eof();
            if (streambuf_type *this_sb = this->rdbuf())
            {
                int_type c                = this_sb->sgetc();
                char_type c2              = traits_type::to_char_type(c);
                unsigned long long gcount = 0;

                while (!traits_type::eq_int_type(c, eof) && !traits_type::eq_int_type(c, idelim) &&
                       !traits_type::eq_int_type(sb.sputc(c2), eof))
                {
                    ++gcount;
                    c  = this_sb->snextc();
                    c2 = traits_type::to_char_type(c);
                }
                if (traits_type::eq_int_type(c, eof))
                    error |= ios_base::eofbit;

                if (gcount <= Detail::__numeric_traits<streamsize>::__max)
                    lgcount = static_cast<streamsize>(gcount);
                else
                    lgcount = Detail::__numeric_traits<streamsize>::__max;
            } else
            {
                error |= ios_base::badbit;
            }
        }
        if (!lgcount)
            error |= ios_base::failbit;
        if (static_cast<bool>(error))
            this->setstate(error);
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::getline(char_type *s, streamsize n,
                                                                                        char_type delim)
    {
        lgcount                 = 0;
        ios_base::iostate error = ios_base::goodbit;
        sentry sy(*this, true);
        if (sy && n > 0)
        {
            const int_type idelim = traits_type::to_int_type(delim);
            const int_type eof    = traits_type::eof();
            if (streambuf_type *sb = this->rdbuf())
            {
                int_type c = sb->sgetc();
                while (lgcount + 1 < n && !traits_type::eq_int_type(c, eof) && !traits_type::eq_int_type(c, idelim))
                {
                    *s++ = traits_type::to_char_type(c);
                    c    = sb->snextc();
                    ++lgcount;
                }
                if (traits_type::eq_int_type(c, eof))
                {
                    error |= ios_base::eofbit;
                } else
                {
                    if (traits_type::eq_int_type(c, idelim))
                    {
                        sb->sbumpc();
                        ++lgcount;
                    } else
                    {
                        error |= ios_base::failbit;
                    }
                }
            } else
            {
                error |= ios_base::badbit;
            }
        }
        if (n > 0)
            *s = char_type();
        if (!lgcount)
            error |= ios_base::failbit;
        if (static_cast<bool>(error))
            this->setstate(error);
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::ignore(void)
    {
        lgcount = 0;
        sentry sy(*this, true);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            const int_type eof      = traits_type::eof();
            if (streambuf_type *sb = this->rdbuf())
            {
                if (traits_type::eq_int_type(sb->sbumpc(), eof))
                    error |= ios_base::eofbit;
                else
                    lgcount = 1;
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::ignore(streamsize n)
    {
        lgcount = 0;
        sentry sy(*this, true);
        if (sy && n > 0)
        {
            ios_base::iostate error = ios_base::goodbit;
            const int_type eof      = traits_type::eof();
            if (streambuf_type *sb = this->rdbuf())
            {
                int_type c        = sb->sgetc();
                bool large_ignore = false;
                while (true)
                {
                    while (lgcount < n && !traits_type::eq_int_type(c, eof))
                    {
                        ++lgcount;
                        c = sb->snextc();
                    }
                    if (n == Detail::__numeric_traits<streamsize>::__max && !traits_type::eq_int_type(c, eof))
                    {
                        lgcount      = Detail::__numeric_traits<streamsize>::__min;
                        large_ignore = true;
                    } else
                    {
                        break;
                    }
                }

                if (n == Detail::__numeric_traits<streamsize>::__max)
                {
                    if (large_ignore)
                        lgcount = Detail::__numeric_traits<streamsize>::__max;

                    if (traits_type::eq_int_type(c, eof))
                        error |= ios_base::eofbit;
                } else if (lgcount < n)
                {
                    if (traits_type::eq_int_type(c, eof))
                        error |= ios_base::eofbit;
                }
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::ignore(streamsize n, int_type delim)
    {
        lgcount = 0;
        sentry sy(*this, true);
        if (sy && n > 0)
        {
            ios_base::iostate error = ios_base::goodbit;
            const int_type eof      = traits_type::eof();
            if (streambuf_type *sb = this->rdbuf())
            {
                int_type c        = sb->sgetc();
                bool large_ignore = false;
                while (true)
                {
                    while (lgcount < n && !traits_type::eq_int_type(c, eof) && !traits_type::eq_int_type(c, delim))
                    {
                        ++lgcount;
                        c = sb->snextc();
                    }
                    if (n == Detail::__numeric_traits<streamsize>::__max && !traits_type::eq_int_type(c, eof) &&
                        !traits_type::eq_int_type(c, delim))
                    {
                        lgcount      = Detail::__numeric_traits<streamsize>::__min;
                        large_ignore = true;
                    } else
                    {
                        break;
                    }
                }

                if (n == Detail::__numeric_traits<streamsize>::__max)
                {
                    if (large_ignore)
                        lgcount = Detail::__numeric_traits<streamsize>::__max;

                    if (traits_type::eq_int_type(c, eof))
                        error |= ios_base::eofbit;
                    else
                    {
                        if (lgcount != n)
                            ++lgcount;
                        sb->sbumpc();
                    }
                } else if (lgcount < n)
                {
                    if (traits_type::eq_int_type(c, eof))
                        error |= ios_base::eofbit;
                    else
                    {
                        ++lgcount;
                        sb->sbumpc();
                    }
                }
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    typename basic_istream<CharacterType, Traits>::int_type basic_istream<CharacterType, Traits>::peek(void)
    {
        int_type c = traits_type::eof();
        lgcount    = 0;
        sentry sy(*this, true);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            if (streambuf_type *sb = this->rdbuf())
            {
                c = sb->sgetc();
                if (traits_type::eq_int_type(c, traits_type::eof()))
                    error |= ios_base::eofbit;
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return c;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::read(char_type *s, streamsize n)
    {
        lgcount = 0;
        sentry sy(*this, true);
        if (sy && n > 0)
        {
            ios_base::iostate error = ios_base::goodbit;
            if (streambuf_type *sb = this->rdbuf())
            {
                lgcount = sb->sgetn(s, n);
                if (lgcount != n)
                    error |= (ios_base::eofbit | ios_base::failbit);
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    streamsize basic_istream<CharacterType, Traits>::readsome(char_type *s, streamsize n)
    {
        lgcount = 0;
        sentry sy(*this, true);
        if (sy && n > 0)
        {
            ios_base::iostate error = ios_base::goodbit;
            if (streambuf_type *sb = this->rdbuf())
            {
                if (const streamsize num = sb->in_avail(); num > 0)
                    lgcount = sb->sgetn(s, (num < n ? num : n));
                else if (num == -1)
                    error |= ios_base::eofbit;
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return lgcount;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::putback(char_type c)
    {
        lgcount = 0;
        this->clear(this->rdstate() & ~ios_base::eofbit);
        sentry sy(*this, true);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            streambuf_type *sb      = this->rdbuf();
            if (!sb || traits_type::eq_int_type(sb->sputbackc(c), traits_type::eof()))
                error |= ios_base::badbit;
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::unget(void)
    {
        lgcount = 0;
        this->clear(this->rdstate() & ~ios_base::eofbit);
        sentry sy(*this, true);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            streambuf_type *sb      = this->rdbuf();
            if (!sb || traits_type::eq_int_type(sb->sungetc(), traits_type::eof()))
                error |= ios_base::badbit;
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    int basic_istream<CharacterType, Traits>::sync(void)
    {
        int ret = -1;
        sentry sy(*this, true);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            if (streambuf_type *sb = this->rdbuf())
            {
                if (sb->pubsync() == -1)
                    error |= ios_base::badbit;
                else
                    ret = 0;
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return ret;
    }

    template<typename CharacterType, typename Traits>
    typename basic_istream<CharacterType, Traits>::pos_type basic_istream<CharacterType, Traits>::tellg(void)
    {
        pos_type ret = pos_type(-1);
        sentry sy(*this, true);
        if (sy)
        {
            if (!this->fail())
            {
                if (streambuf_type *sb = this->rdbuf())
                    ret = sb->pubseekoff(0, ios_base::cur, ios_base::in);
            }
        }
        return ret;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::seekg(pos_type pos)
    {
        this->clear(this->rdstate() & ~ios_base::eofbit);
        if (const sentry sy(*this, true); sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            if (!this->fail())
            {
                if (streambuf_type *sb = this->rdbuf())
                {
                    const pos_type p = sb->pubseekpos(pos, ios_base::in);
                    if (p == pos_type(off_type(-1)))
                        error |= ios_base::failbit;
                } else
                {
                    error |= ios_base::badbit;
                }
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &basic_istream<CharacterType, Traits>::seekg(off_type off,
                                                                                      ios_base::seekdir dir)
    {
        this->clear(this->rdstate() & ~ios_base::eofbit);
        sentry sy(*this, true);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            if (!this->fail())
            {
                if (streambuf_type *sb = this->rdbuf())
                {
                    const pos_type p = sb->pubseekoff(off, dir, ios_base::in);
                    if (p == pos_type(off_type(-1)))
                        error |= ios_base::failbit;
                } else
                {
                    error |= ios_base::badbit;
                }
            }
            if (static_cast<bool>(error))
                this->setstate(error);
        }
        return *this;
    }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &operator>>(basic_istream<CharacterType, Traits> &in, CharacterType &c)
    {
        typedef basic_istream<CharacterType, Traits> istream_type;
        typedef typename istream_type::int_type int_type;
        typedef typename istream_type::streambuf_type streambuf_type;

        if (typename istream_type::sentry sy(in, false); sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            if (streambuf_type *sb = in.rdbuf())
            {
                const int_type cb = sb->sbumpc();
                if (!Traits::eq_int_type(cb, Traits::eof()))
                    c = Traits::to_char_type(cb);
                else
                    error |= (ios_base::eofbit | ios_base::failbit);
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                in.setstate(error);
        }
        return in;
    }

    template<typename CharacterType, typename Traits>
    void istream_extract(basic_istream<CharacterType, Traits> &in, CharacterType *s, streamsize num)
    {
        typedef basic_istream<CharacterType, Traits> istream_type;
        typedef basic_streambuf<CharacterType, Traits> streambuf_type;
        typedef typename Traits::int_type int_type;
        typedef CharacterType char_type;
        typedef ctype<CharacterType> ctype_type;

        streamsize extracted    = 0;
        ios_base::iostate error = ios_base::goodbit;
        typename istream_type::sentry sy(in, false);
        if (sy && num > 0)
        {
            streamsize width = in.width();
            if (0 < width && width < num)
                num = width;

            const ctype_type &ct = check_facet(in.get_ctype());
            const int_type eof   = Traits::eof();
            if (streambuf_type *sb = in.rdbuf())
            {
                int_type c = sb->sgetc();
                while (extracted < num - 1 && !Traits::eq_int_type(c, eof) &&
                       !ct.is(ctype_base::space, Traits::to_char_type(c)))
                {
                    *s++ = Traits::to_char_type(c);
                    ++extracted;
                    c = sb->snextc();
                }

                if (extracted < num - 1 && Traits::eq_int_type(c, eof))
                    error |= ios_base::eofbit;

                *s = char_type();
                in.width(0);
            } else
            {
                error |= ios_base::badbit;
            }
        }
        if (!extracted)
            error |= ios_base::failbit;
        if (static_cast<bool>(error))
            in.setstate(error);
    }

    void istream_extract(istream &in, char *s, streamsize num) { istream_extract<char, char_traits<char>>(in, s, num); }

    template<typename CharacterType, typename Traits>
    basic_istream<CharacterType, Traits> &ws(basic_istream<CharacterType, Traits> &in)
    {
        typedef basic_istream<CharacterType, Traits> istream_type;
        typedef basic_streambuf<CharacterType, Traits> streambuf_type;
        typedef typename istream_type::int_type int_type;
        typedef ctype<CharacterType> ctype_type;

        typename istream_type::sentry sy(in, true);
        if (sy)
        {
            ios_base::iostate error = ios_base::goodbit;
            const ctype_type &ct    = check_facet(in.get_ctype());
            const int_type eof      = Traits::eof();
            if (streambuf_type *sb = in.rdbuf())
            {
                int_type c = sb->sgetc();
                while (true)
                {
                    if (Traits::eq_int_type(c, eof))
                    {
                        error = ios_base::eofbit;
                        break;
                    }
                    if (!ct.is(ctype_base::space, Traits::to_char_type(c)))
                        break;
                    c = sb->snextc();
                }
            } else
            {
                error |= ios_base::badbit;
            }
            if (static_cast<bool>(error))
                in.setstate(error);
        }
        return in;
    }
} // namespace SFTL
