#pragma once

#include "../Containers/Deque.hpp"
#include "../TypeTraits.hpp"
#include "IOS.hpp"

namespace SFTL
{
#define IsUnused __attribute__((__unused__))
    template<typename CharacterType, typename Traits>
    streamsize copy_streambufs_eof(basic_streambuf<CharacterType, Traits> *, basic_streambuf<CharacterType, Traits> *,
                                   bool &);

    template<typename CharacterType, typename Traits>
    class basic_streambuf
    {
    public:
        virtual ~basic_streambuf() = default;
        typedef CharacterType char_type;
        typedef Traits traits_type;
        typedef traits_type::int_type int_type;
        typedef traits_type::pos_type pos_type;
        typedef traits_type::off_type off_type;


        friend class basic_ios<char_type, traits_type>;
        friend class basic_istream<char_type, traits_type>;
        friend class basic_ostream<char_type, traits_type>;
        friend class istreambuf_iterator<char_type, traits_type>;
        friend class ostreambuf_iterator<char_type, traits_type>;

        friend streamsize copy_streambufs_eof<>(basic_streambuf *, basic_streambuf *, bool &);

        template<typename CharacterType2>
        friend enable_if<is_char<CharacterType2>::value, istreambuf_iterator<CharacterType2>>::type
        find(istreambuf_iterator<CharacterType2>, istreambuf_iterator<CharacterType2>, const CharacterType2 &);

        template<typename CharacterType2, typename Dist>
        friend enable_if<is_char<CharacterType2>::value, void>::type advance(istreambuf_iterator<CharacterType2> &,
                                                                             Dist);

        friend void istream_extract(istream &, char *, streamsize);

        template<typename CharacterType2, typename Traits2>
        friend basic_istream<CharacterType2, Traits2> &operator>>(basic_istream<CharacterType2, Traits2> &,
                                                                  AdvancedString<CharacterType2, Traits2> &);

        template<typename CharacterType2, typename Traits2>
        friend basic_istream<CharacterType2, Traits2> &
        getline(basic_istream<CharacterType2, Traits2> &, AdvancedString<CharacterType2, Traits2> &, CharacterType2);

    protected:
        char_type *inbeg;
        char_type *incur;
        char_type *inend;
        char_type *outbegin;
        char_type *outcur;
        char_type *outend;

        locale bufflocale;

    public:
        locale pubimbue(const locale &loc)
        {
            locale tmp(this->getloc());
            this->imbue(loc);
            bufflocale = loc;
            return tmp;
        }

        [[nodiscard]] locale getloc() const { return bufflocale; }

        basic_streambuf *pubsetbuf(char_type *ct, streamsize n) { return this->setbuf(ct, n); }

        pos_type pubseekoff(off_type off, ios_base::seekdir way, ios_base::openmode mode = ios_base::in | ios_base::out)
        {
            return this->seekoff(off, way, mode);
        }

        pos_type pubseekpos(pos_type ctp, ios_base::openmode mode = ios_base::in | ios_base::out)
        {
            return this->seekpos(ctp, mode);
        }

        int pubsync() { return this->sync(); }

        streamsize in_avail()
        {
            const streamsize ret = this->egptr() - this->gptr();
            return ret ? ret : this->showmanyc();
        }

        int_type snextc()
        {
            int_type ret = traits_type::eof();
            if (!traits_type::eq_int_type(this->sbumpc(), ret), true) [[likely]]
                ret = this->sgetc();
            return ret;
        }

        int_type sbumpc()
        {
            int_type ret;
            if (this->gptr() < this->egptr(), true) [[likely]]
            {
                ret = traits_type::to_int_type(*this->gptr());
                this->gbump(1);
            } else
                ret = this->uflow();
            return ret;
        }

        int_type sgetc()
        {
            int_type ret;
            if (this->gptr() < this->egptr(), true) [[likely]]
                ret = traits_type::to_int_type(*this->gptr());
            else
                ret = this->underflow();
            return ret;
        }

        streamsize sgetn(char_type *ct, streamsize n) { return this->xsgetn(ct, n); }

        int_type sputbackc(char_type c)
        {
            int_type ret;
            const bool testpos = this->eback() < this->gptr();
            if (!testpos || !traits_type::eq(c, this->gptr()[-1]), false) [[likely]]
                ret = this->pbackfail(traits_type::to_int_type(c));
            else
            {
                this->gbump(-1);
                ret = traits_type::to_int_type(*this->gptr());
            }
            return ret;
        }
        int_type sungetc()
        {
            int_type ret;
            if (this->eback() < this->gptr(), true) [[likely]]
            {
                this->gbump(-1);
                ret = traits_type::to_int_type(*this->gptr());
            } else
                ret = this->pbackfail();
            return ret;
        }


        int_type sputc(char_type c)
        {
            int_type ret;
            if (this->pptr() < this->epptr(), true) [[likely]]
            {
                *this->pptr() = c;
                this->pbump(1);
                ret = traits_type::to_int_type(c);
            } else
                ret = this->overflow(traits_type::to_int_type(c));
            return ret;
        }

        streamsize sputn(const char_type *ct, streamsize n) { return this->xsputn(ct, n); }

    protected:
        basic_streambuf() :
            inbeg(nullptr), incur(nullptr), inend(nullptr), outbegin(nullptr), outcur(nullptr), outend(nullptr),
            bufflocale(locale())
        {
        }


        char_type *eback() const { return inbeg; }
        char_type *gptr() const { return incur; }
        char_type *egptr() const { return inend; }
        void gbump(int n) { incur += n; }
        void setg(char_type *gbeg, char_type *gnext, char_type *gend)
        {
            inbeg = gbeg;
            incur = gnext;
            inend = gend;
        }

        char_type *pbase() const { return outbegin; }
        char_type *pptr() const { return outcur; }
        char_type *epptr() const { return outend; }
        void pbump(int n) { outcur += n; }

        void setp(char_type *pbeg, char_type *pend)
        {
            outbegin = outcur = pbeg;
            outend            = pend;
        }

        void imbue(const locale &loc IsUnused) {}

        basic_streambuf<char_type, Traits> *setbuf(char_type *, streamsize) { return this; }

        pos_type seekoff(off_type, ios_base::seekdir, ios_base::openmode /*mode*/ = ios_base::in | ios_base::out)
        {
            return pos_type(off_type(-1));
        }
        pos_type seekpos(pos_type, ios_base::openmode /*mode*/ = ios_base::in | ios_base::out)
        {
            return pos_type(off_type(-1));
        }

        int sync() { return 0; }
        streamsize showmanyc() { return 0; }
        streamsize xsgetn(char_type *ct, streamsize n);

        int_type underflow() { return traits_type::eof(); }
        int_type uflow()
        {
            int_type ret       = traits_type::eof();
            const bool testEOF = traits_type::eq_int_type(this->underflow(), ret);
            if (!testEOF)
            {
                ret = traits_type::to_int_type(*this->gptr());
                this->gbump(1);
            }
            return ret;
        }

        int_type pbackfail(int_type c IsUnused = traits_type::eof()) { return traits_type::eof(); }
        streamsize xsputn(const char_type *ct, streamsize n);
        int_type overflow(int_type c IsUnused = traits_type::eof()) { return traits_type::eof(); }

        void safe_gbump(streamsize n) { incur += n; }
        void safe_pbump(streamsize n) { outcur += n; }

    protected:
        basic_streambuf(const basic_streambuf &);

        basic_streambuf &operator=(const basic_streambuf &);

        void swap(basic_streambuf &ctb) noexcept
        {
            ::SFTL::swap(inbeg, ctb.inbeg);
            ::SFTL::swap(incur, ctb.incur);
            ::SFTL::swap(inend, ctb.inend);
            ::SFTL::swap(outbegin, ctb.outbegin);
            ::SFTL::swap(outcur, ctb.outcur);
            ::SFTL::swap(outend, ctb.outend);
            ::SFTL::swap(bufflocale, ctb.bufflocale);
        }
    };

    template<typename CharacterType, typename Traits>
    ::SFTL::basic_streambuf<CharacterType, Traits>::basic_streambuf(const basic_streambuf &) = default;

    template<typename CharacterType, typename Traits>
    ::SFTL::basic_streambuf<CharacterType, Traits> & ::SFTL::basic_streambuf<CharacterType, Traits>::operator=(
            const basic_streambuf &) = default;

    template<>
    streamsize copy_streambufs_eof(basic_streambuf<char> *ctbin, basic_streambuf<char> *ctbout, bool &eof);

    template<>
    streamsize copy_streambufs_eof(basic_streambuf<wchar_t> *ctbin, basic_streambuf<wchar_t> *ctbout, bool &eof);

#undef IsUnused

} // namespace SFTL
