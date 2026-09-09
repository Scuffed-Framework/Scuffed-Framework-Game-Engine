#pragma once
#include "InOutStream.hpp"

namespace SFTL
{
    template<typename CharacterType, typename Traits, typename Allocator>
    class basic_stringbuf : public basic_streambuf<CharacterType, Traits>
    {
        struct xfer_bufptrs;

        using allocator_traits = allocator_traits<Allocator>;
        using noexcept_swap =
                Or<typename allocator_traits::propagate_on_container_swap, typename allocator_traits::is_always_equal>;

    public:
        typedef CharacterType char_type;
        typedef Traits traits_type;
        typedef Allocator allocator_type;
        typedef traits_type::int_type int_type;
        typedef traits_type::pos_type pos_type;
        typedef traits_type::off_type off_type;

        typedef basic_streambuf<char_type, traits_type> streambuf_type;
        typedef AdvancedString<char_type, Traits> string_type;

    protected:
        ios_base::openmode lmode;
        string_type lstring;

    public:
        basic_stringbuf() : streambuf_type(), lmode(ios_base::in | ios_base::out), lstring() {}


        explicit basic_stringbuf(ios_base::openmode mode) : streambuf_type(), lmode(mode), lstring() {}


        explicit basic_stringbuf(const string_type &str, ios_base::openmode mode = ios_base::in | ios_base::out) :
            streambuf_type(), lmode(), lstring(str.data(), str.size(), str.get_allocator())
        {
            lstringbuf_init(mode);
        }
        basic_stringbuf(const basic_stringbuf &) = delete;

        basic_stringbuf(basic_stringbuf &&RightHandSide) noexcept :
            basic_stringbuf(move(RightHandSide), xfer_bufptrs(RightHandSide, this))
        {
            RightHandSide.lsync(const_cast<char_type *>(RightHandSide.lstring.data()), 0, 0);
        }

        explicit basic_stringbuf(const allocator_type &a) : basic_stringbuf(ios_base::in | ios_base::out, a) {}

        basic_stringbuf(ios_base::openmode mode, const allocator_type &a) : streambuf_type(), lmode(mode), lstring(a) {}

        explicit basic_stringbuf(string_type &&s, ios_base::openmode mode = ios_base::in | ios_base::out) :
            streambuf_type(), lmode(mode), lstring(move(s))
        {
            lstringbuf_init(mode);
        }

        template<typename Alloc>
        basic_stringbuf(const AdvancedString<CharacterType, Alloc> &s, const allocator_type &a) :
            basic_stringbuf(s, ios_base::in | ios_base::out, a)
        {
        }

        template<typename Alloc>
        basic_stringbuf(const AdvancedString<CharacterType, Alloc> &s, ios_base::openmode mode,
                        const allocator_type &a) : streambuf_type(), lmode(mode), lstring(s.data(), s.size(), a)
        {
            lstringbuf_init(mode);
        }

        template<typename Alloc>
        explicit basic_stringbuf(const AdvancedString<CharacterType, Alloc> &s,
                                 ios_base::openmode mode = ios_base::in | ios_base::out) :
            basic_stringbuf(s, mode, allocator_type{})
        {
        }

        template<typename Type>
        explicit basic_stringbuf(const Type &t, ios_base::openmode mode = ios_base::in | ios_base::out)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : basic_stringbuf(t, mode, allocator_type{})
        {
        }

        template<typename Type>
        basic_stringbuf(const Type &t, const allocator_type &a)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : basic_stringbuf(t, ios_base::in | ios_base::out, a)
        {
        }

        template<typename Type>
        basic_stringbuf(const Type &t, ios_base::openmode mode, const allocator_type &a)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : lstring(t, a)
        {
            lstringbuf_init(mode);
        }

        basic_stringbuf(basic_stringbuf &&RightHandSide, const allocator_type &a) :
            basic_stringbuf(move(RightHandSide), a, xfer_bufptrs(RightHandSide, this))
        {
            RightHandSide.lsync(const_cast<char_type *>(RightHandSide.lstring.data()), 0, 0);
        }

        allocator_type get_allocator() const noexcept { return lstring.get_allocator(); }


        basic_stringbuf &operator=(const basic_stringbuf &) = delete;

        basic_stringbuf &operator=(basic_stringbuf &&RightHandSide) noexcept
        {
            xfer_bufptrs st{RightHandSide, this};
            const streambuf_type &base = RightHandSide;
            streambuf_type::operator=(base);
            this->pubimbue(RightHandSide.getloc());
            lmode   = RightHandSide.lmode;
            lstring = move(RightHandSide.lstring);
            RightHandSide.lsync(const_cast<char_type *>(RightHandSide.lstring.data()), 0, 0);
            return *this;
        }

        void swap(basic_stringbuf &RightHandSide) noexcept(noexcept_swap::value)
        {
            xfer_bufptrs l_st{*this, addressof(RightHandSide)};
            xfer_bufptrs r_st{RightHandSide, this};
            streambuf_type &base = RightHandSide;
            streambuf_type::swap(base);
            RightHandSide.pubimbue(this->pubimbue(RightHandSide.getloc()));
            swap(lmode, RightHandSide.lmode);
            swap(lstring, RightHandSide.lstring);
        }

        [[nodiscard]] string_type str() const
        {
            string_type ret(lstring.get_allocator());
            if (char_type *hi = lhigh_mark())
                ret.assign(this->pbase(), hi);
            else
                ret = lstring;
            return ret;
        }

        template<allocator_like Alloc>
        [[nodiscard]] AdvancedString<CharacterType, Alloc> str(const Alloc &sa) const
        {
            auto sv = view();
            return {sv.data(), sv.size(), sa};
        }

        [[nodiscard]] string_type str()
        {
            if (char_type *hi = lhigh_mark())
            {
                if (lstring.data() == this->pbase()) [[likely]]
                    lstring.lset_length(hi - this->pbase());
                else
                    lstring.assign(this->pbase(), hi);
            }
            auto str = move(lstring);
            lstring.clear();
            lsync(lstring.data(), 0, 0);
            return str;
        }

        AdvancedString<char_type> view() noexcept
        {
            if (char_type *hi = lhigh_mark())
                return {this->pbase(), hi};
            else
                return lstring;
        }

        void str(const string_type &s)
        {
            lstring.assign(s.data(), s.size());
            lstringbuf_init(lmode);
        }

        template<allocator_like Alloc>
            requires(!is_same_v<Alloc, Allocator>)
        void str(const AdvancedString<CharacterType, Alloc> &s)
        {
            lstring.assign(s.data(), s.size());
            lstringbuf_init(lmode);
        }

        void str(string_type &&s)
        {
            lstring = move(s);
            lstringbuf_init(lmode);
        }

        template<typename Type>
        void str(const Type &t)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
        {
            AdvancedStringView<CharacterType> sv{t};
            lstring = sv;
            lstringbuf_init(lmode);
        }

    protected:
        void lstringbuf_init(ios_base::openmode mode)
        {
            lmode         = mode;
            size_type len = 0;
            if (static_cast<bool>(lmode & (ios_base::ate | ios_base::app)))
                len = lstring.size();
            lsync(const_cast<char_type *>(lstring.data()), 0, len);
        }

        streamsize showmanyc()
        {
            streamsize ret = -1;
            if (static_cast<bool>(lmode & ios_base::in))
            {
                lupdate_egptr();
                ret = this->egptr() - this->gptr();
            }
            return ret;
        }

        int_type underflow();

        int_type pbackfail(int_type c = traits_type::eof());

        int_type overflow(int_type c = traits_type::eof());

        streambuf_type *setbuf(char_type *s, streamsize n)
        {
            if (s && n >= 0)
            {
                lstring.clear();

                lsync(s, n, 0);
            }
            return this;
        }

        pos_type seekoff(off_type off, ios_base::seekdir way, ios_base::openmode mode = ios_base::in | ios_base::out);

        pos_type seekpos(pos_type sp, ios_base::openmode mode = ios_base::in | ios_base::out);

        void lsync(char_type *base, size_type i, size_type o);

        void lupdate_egptr()
        {
            if (char_type *pptr = this->pptr())
            {
                char_type *egptr = this->egptr();
                if (!egptr || pptr > egptr)
                {
                    if (static_cast<bool>(lmode & ios_base::in))
                        this->setg(this->eback(), this->gptr(), pptr);
                    else
                        this->setg(pptr, pptr, pptr);
                }
            }
        }

        void lpbump(char_type *pbeg, char_type *pend, off_type off);

    private:
        char_type *lhigh_mark() const noexcept
        {
            if (char_type *pptr = this->pptr())
            {
                char_type *egptr = this->egptr();
                if (!egptr || pptr > egptr)
                    return pptr;
                return egptr;
            }
            return nullptr;
        }

        struct xfer_bufptrs
        {
            xfer_bufptrs(const basic_stringbuf &from, basic_stringbuf *to) :
                lto{to}, lgoff{-1, -1, -1}, lpoff{-1, -1, -1}
            {
                const CharacterType *const str = from.lstring.data();
                const CharacterType *end       = nullptr;
                if (from.eback())
                {
                    lgoff[0] = from.eback() - str;
                    lgoff[1] = from.gptr() - str;
                    lgoff[2] = from.egptr() - str;
                    end      = from.egptr();
                }
                if (from.pbase())
                {
                    lpoff[0] = from.pbase() - str;
                    lpoff[1] = from.pptr() - from.pbase();
                    lpoff[2] = from.epptr() - str;
                    if (!end || from.pptr() > end)
                        end = from.pptr();
                }

                if (end)
                {
                    auto &mut_from = const_cast<basic_stringbuf &>(from);
                    mut_from.lstring.llength(end - str);
                }
            }

            ~xfer_bufptrs()
            {
                char_type *str = const_cast<char_type *>(lto->lstring.data());
                if (lgoff[0] != -1)
                    lto->setg(str + lgoff[0], str + lgoff[1], str + lgoff[2]);
                if (lpoff[0] != -1)
                    lto->lpbump(str + lpoff[0], str + lpoff[2], lpoff[1]);
            }

            basic_stringbuf *lto;
            off_type lgoff[3];
            off_type lpoff[3];
        };
        basic_stringbuf(basic_stringbuf &&RightHandSide, xfer_bufptrs &&) :
            streambuf_type(static_cast<const streambuf_type &>(RightHandSide)), lmode(RightHandSide.lmode),
            lstring(move(RightHandSide.lstring))
        {
        }

        basic_stringbuf(basic_stringbuf &&RightHandSide, const allocator_type &a, xfer_bufptrs &&) :
            streambuf_type(static_cast<const streambuf_type &>(RightHandSide)), lmode(RightHandSide.lmode),
            lstring(move(RightHandSide.lstring), a)
        {
        }
    };


    template<typename CharacterType, typename Traits, typename _Alloc>
    class basic_istringstream : public basic_istream<CharacterType, Traits>
    {
    public:
        typedef CharacterType char_type;
        typedef Traits traits_type;
        typedef _Alloc allocator_type;
        typedef traits_type::int_type int_type;
        typedef traits_type::pos_type pos_type;
        typedef traits_type::off_type off_type;

        typedef AdvancedString<CharacterType, _Alloc> string_type;
        typedef basic_stringbuf<CharacterType, Traits, _Alloc> stringbuf_type;
        typedef basic_istream<char_type, traits_type> istream_type;

    private:
        stringbuf_type lstringbuf;

    public:
        basic_istringstream() : istream_type(), lstringbuf(ios_base::in) { this->init(addressof(lstringbuf)); }

        explicit basic_istringstream(ios_base::openmode mode) : istream_type(), lstringbuf(mode | ios_base::in)
        {
            this->init(addressof(lstringbuf));
        }


        explicit basic_istringstream(const string_type &str, ios_base::openmode mode = ios_base::in) :
            istream_type(), lstringbuf(str, mode | ios_base::in)
        {
            this->init(addressof(lstringbuf));
        }

        ~basic_istringstream() {}

        basic_istringstream(const basic_istringstream &) = delete;

        basic_istringstream(basic_istringstream &&RightHandSide) noexcept :
            istream_type(move(RightHandSide)), lstringbuf(move(RightHandSide.lstringbuf))
        {
            istream_type::set_rdbuf(addressof(lstringbuf));
        }
        basic_istringstream(ios_base::openmode mode, const allocator_type &a) :
            istream_type(), lstringbuf(mode | ios_base::in, a)
        {
            this->init(addressof(lstringbuf));
        }

        explicit basic_istringstream(string_type &&str, ios_base::openmode mode = ios_base::in) :
            istream_type(), lstringbuf(move(str), mode | ios_base::in)
        {
            this->init(addressof(lstringbuf));
        }

        template<typename Alloc>
        basic_istringstream(const AdvancedString<CharacterType, Alloc> &str, const allocator_type &a) :
            basic_istringstream(str, ios_base::in, a)
        {
        }

        template<typename Alloc>
        basic_istringstream(const AdvancedString<CharacterType, Alloc> &str, ios_base::openmode mode,
                            const allocator_type &a) : istream_type(), lstringbuf(str, mode | ios_base::in, a)
        {
            this->init(addressof(lstringbuf));
        }

        template<typename Alloc>
        explicit basic_istringstream(const AdvancedString<CharacterType, Alloc> &str,
                                     ios_base::openmode mode = ios_base::in) :
            basic_istringstream(str, mode, allocator_type())
        {
        }
        template<typename Type>
        explicit basic_istringstream(const Type &t, ios_base::openmode mode = ios_base::in)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : basic_istringstream(t, mode, allocator_type{})
        {
        }

        template<typename Type>
        basic_istringstream(const Type &t, const allocator_type &a)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : basic_istringstream(t, ios_base::in, a)
        {
        }

        template<typename Type>
        basic_istringstream(const Type &t, ios_base::openmode mode, const allocator_type &a)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : istream_type(), lstringbuf(t, mode | ios_base::in, a)
        {
            this->init(addressof(lstringbuf));
        }


        basic_istringstream &operator=(const basic_istringstream &) = delete;

        basic_istringstream &operator=(basic_istringstream &&RightHandSide) noexcept
        {
            istream_type::operator=(move(RightHandSide));
            lstringbuf = move(RightHandSide.lstringbuf);
            return *this;
        }

        void swap(basic_istringstream &RightHandSide) noexcept
        {
            istream_type::swap(RightHandSide);
            lstringbuf.swap(RightHandSide.lstringbuf);
        }

        [[nodiscard]] stringbuf_type *rdbuf() const { return const_cast<stringbuf_type *>(addressof(lstringbuf)); }

        [[nodiscard]] string_type str() const { return lstringbuf.str(); }

        template<allocator_like Alloc>
        [[nodiscard]] AdvancedString<CharacterType, Alloc> str(const Alloc &sa) const
        {
            return lstringbuf.str(sa);
        }

        [[nodiscard]] string_type str() { return move(lstringbuf).str(); }
        AdvancedStringView<char_type> view() const noexcept { return lstringbuf.view(); }

        void str(const string_type &s) { lstringbuf.str(s); }

        template<allocator_like Alloc>
            requires(!is_same_v<Alloc, _Alloc>)
        void str(const AdvancedString<CharacterType, Alloc> &s)
        {
            lstringbuf.str(s);
        }

        void str(string_type &&s) { lstringbuf.str(move(s)); }

        template<typename Type>
        void str(const Type &t)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
        {
            lstringbuf.str(t);
        }
    };

    template<typename CharacterType, typename Traits, typename _Alloc>
    class basic_ostringstream : public basic_ostream<CharacterType, Traits>
    {
    public:
        typedef CharacterType char_type;
        typedef Traits traits_type;
        typedef _Alloc allocator_type;
        typedef traits_type::int_type int_type;
        typedef traits_type::pos_type pos_type;
        typedef traits_type::off_type off_type;

        typedef AdvancedString<CharacterType, _Alloc> string_type;
        typedef basic_stringbuf<CharacterType, Traits, _Alloc> stringbuf_type;
        typedef basic_ostream<char_type, traits_type> ostream_type;

    private:
        stringbuf_type lstringbuf;

    public:
        basic_ostringstream() : ostream_type(), lstringbuf(ios_base::out) { this->init(addressof(lstringbuf)); }


        explicit basic_ostringstream(ios_base::openmode mode) : ostream_type(), lstringbuf(mode | ios_base::out)
        {
            this->init(addressof(lstringbuf));
        }


        explicit basic_ostringstream(const string_type &str, ios_base::openmode mode = ios_base::out) :
            ostream_type(), lstringbuf(str, mode | ios_base::out)
        {
            this->init(addressof(lstringbuf));
        }

        ~basic_ostringstream() override {}

        basic_ostringstream(const basic_ostringstream &) = delete;

        basic_ostringstream(basic_ostringstream &&RightHandSide) noexcept :
            ostream_type(move(RightHandSide)), lstringbuf(move(RightHandSide.lstringbuf))
        {
            ostream_type::set_rdbuf(addressof(lstringbuf));
        }
        basic_ostringstream(ios_base::openmode mode, const allocator_type &a) :
            ostream_type(), lstringbuf(mode | ios_base::out, a)
        {
            this->init(addressof(lstringbuf));
        }

        explicit basic_ostringstream(string_type &&str, ios_base::openmode mode = ios_base::out) :
            ostream_type(), lstringbuf(move(str), mode | ios_base::out)
        {
            this->init(addressof(lstringbuf));
        }

        template<typename Alloc>
        basic_ostringstream(const AdvancedString<CharacterType, Alloc> &str, const allocator_type &a) :
            basic_ostringstream(str, ios_base::out, a)
        {
        }

        template<typename Alloc>
        basic_ostringstream(const AdvancedString<CharacterType, Alloc> &str, ios_base::openmode mode,
                            const allocator_type &a) : ostream_type(), lstringbuf(str, mode | ios_base::out, a)
        {
            this->init(addressof(lstringbuf));
        }

        template<typename Alloc>
        explicit basic_ostringstream(const AdvancedString<CharacterType, Alloc> &str,
                                     ios_base::openmode mode = ios_base::out) :
            basic_ostringstream(str, mode, allocator_type())
        {
        }
        template<typename Type>
        explicit basic_ostringstream(const Type &t, ios_base::openmode mode = ios_base::out)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : basic_ostringstream(t, mode, allocator_type{})
        {
        }

        template<typename Type>
        basic_ostringstream(const Type &t, const allocator_type &a)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : basic_ostringstream(t, ios_base::out, a)
        {
        }

        template<typename Type>
        basic_ostringstream(const Type &t, ios_base::openmode mode, const allocator_type &a)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : ostream_type(), lstringbuf(t, mode | ios_base::out, a)
        {
            this->init(addressof(lstringbuf));
        }

        basic_ostringstream &operator=(const basic_ostringstream &) = delete;

        basic_ostringstream &operator=(basic_ostringstream &&RightHandSide) noexcept
        {
            ostream_type::operator=(move(RightHandSide));
            lstringbuf = move(RightHandSide.lstringbuf);
            return *this;
        }

        void swap(basic_ostringstream &RightHandSide) noexcept
        {
            ostream_type::swap(RightHandSide);
            lstringbuf.swap(RightHandSide.lstringbuf);
        }

        [[nodiscard]] stringbuf_type *rdbuf() const { return const_cast<stringbuf_type *>(addressof(lstringbuf)); }

        [[nodiscard]] string_type str() const { return lstringbuf.str(); }

        template<allocator_like Alloc>
        [[nodiscard]] AdvancedString<CharacterType, Alloc> str(const Alloc &sa) const
        {
            return lstringbuf.str(sa);
        }

        [[nodiscard]] string_type str() { return move(lstringbuf).str(); }


        AdvancedString<char_type> view() const noexcept { return lstringbuf.view(); }

        void str(const string_type &s) { lstringbuf.str(s); }

        template<allocator_like Alloc>
            requires(!is_same_v<Alloc, _Alloc>)
        void str(const AdvancedString<CharacterType, Alloc> &s)
        {
            lstringbuf.str(s);
        }

        void str(string_type &&s) { lstringbuf.str(move(s)); }

        template<typename Type>
        void str(const Type &t)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
        {
            lstringbuf.str(t);
        }
    };

    template<typename CharacterType, typename Traits, typename _Alloc>
    class basic_stringstream : public basic_iostream<CharacterType, Traits>
    {
    public:
        typedef CharacterType char_type;
        typedef Traits traits_type;
        typedef _Alloc allocator_type;
        typedef traits_type::int_type int_type;
        typedef traits_type::pos_type pos_type;
        typedef traits_type::off_type off_type;

        typedef AdvancedString<CharacterType, _Alloc> string_type;
        typedef basic_stringbuf<CharacterType, Traits, _Alloc> stringbuf_type;
        typedef basic_iostream<char_type, traits_type> iostream_type;

    private:
        stringbuf_type lstringbuf;

    public:
        basic_stringstream() : iostream_type(), lstringbuf(ios_base::out | ios_base::in)
        {
            this->init(addressof(lstringbuf));
        }

        explicit basic_stringstream(ios_base::openmode m) : iostream_type(), lstringbuf(m)
        {
            this->init(addressof(lstringbuf));
        }


        explicit basic_stringstream(const string_type &str, ios_base::openmode m = ios_base::out | ios_base::in) :
            iostream_type(), lstringbuf(str, m)
        {
            this->init(addressof(lstringbuf));
        }

        ~basic_stringstream() override {}

        basic_stringstream(const basic_stringstream &) = delete;

        basic_stringstream(basic_stringstream &&RightHandSide) noexcept :
            iostream_type(move(RightHandSide)), lstringbuf(move(RightHandSide.lstringbuf))
        {
            iostream_type::set_rdbuf(addressof(lstringbuf));
        }

        basic_stringstream(ios_base::openmode mode, const allocator_type &a) : iostream_type(), lstringbuf(mode, a)
        {
            this->init(addressof(lstringbuf));
        }

        explicit basic_stringstream(string_type &&str, ios_base::openmode mode = ios_base::in | ios_base::out) :
            iostream_type(), lstringbuf(move(str), mode)
        {
            this->init(addressof(lstringbuf));
        }

        template<typename Alloc>
        basic_stringstream(const AdvancedString<CharacterType, Alloc> &str, const allocator_type &a) :
            basic_stringstream(str, ios_base::in | ios_base::out, a)
        {
        }

        template<typename Alloc>
        basic_stringstream(const AdvancedString<CharacterType, Alloc> &str, ios_base::openmode mode,
                           const allocator_type &a) : iostream_type(), lstringbuf(str, mode, a)
        {
            this->init(addressof(lstringbuf));
        }

        template<typename Alloc>
        explicit basic_stringstream(const AdvancedString<CharacterType, Alloc> &str,
                                    ios_base::openmode mode = ios_base::in | ios_base::out) :
            basic_stringstream(str, mode, allocator_type())
        {
        }
        template<typename Type>
        explicit basic_stringstream(const Type &t, ios_base::openmode mode = ios_base::in | ios_base::out)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : basic_stringstream(t, mode, allocator_type{})
        {
        }

        template<typename Type>
        basic_stringstream(const Type &t, const allocator_type &a)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : basic_stringstream(t, ios_base::in | ios_base::out, a)
        {
        }

        template<typename Type>
        basic_stringstream(const Type &t, ios_base::openmode mode, const allocator_type &a)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
            : iostream_type(), lstringbuf(t, mode, a)
        {
            this->init(addressof(lstringbuf));
        }
        basic_stringstream &operator=(const basic_stringstream &) = delete;

        basic_stringstream &operator=(basic_stringstream &&RightHandSide) noexcept
        {
            iostream_type::operator=(move(RightHandSide));
            lstringbuf = move(RightHandSide.lstringbuf);
            return *this;
        }

        void swap(basic_stringstream &RightHandSide) noexcept
        {
            iostream_type::swap(RightHandSide);
            lstringbuf.swap(RightHandSide.lstringbuf);
        }


        [[nodiscard]] stringbuf_type *rdbuf() const { return const_cast<stringbuf_type *>(addressof(lstringbuf)); }
        [[nodiscard]] string_type str() const { return lstringbuf.str(); }

        template<allocator_like Alloc>
        [[nodiscard]] AdvancedString<CharacterType, Alloc> str(const Alloc &sa) const
        {
            return lstringbuf.str(sa);
        }

        [[nodiscard]] string_type str() { return move(lstringbuf).str(); }


        AdvancedString<char_type> view() const noexcept { return lstringbuf.view(); }

        template<allocator_like Alloc>
            requires(!is_same_v<Alloc, _Alloc>)
        void str(const AdvancedString<CharacterType, Alloc> &s)
        {
            lstringbuf.str(s);
        }
        void str(string_type &&s) { lstringbuf.str(move(s)); }

        template<typename Type>
        void str(const Type &t)
            requires(is_convertible_v<const Type &, AdvancedStringView<CharacterType>>)
        {
            lstringbuf.str(t);
        }
    };

    template<class CharacterType, class Traits, class Allocator>
    inline void swap(basic_stringbuf<CharacterType, Traits, Allocator> &x,
                     basic_stringbuf<CharacterType, Traits, Allocator> &y) noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }

    template<class CharacterType, class Traits, class Allocator>
    inline void swap(basic_istringstream<CharacterType, Traits, Allocator> &x,
                     basic_istringstream<CharacterType, Traits, Allocator> &y) noexcept
    {
        x.swap(y);
    }

    template<class CharacterType, class Traits, class Allocator>
    inline void swap(basic_ostringstream<CharacterType, Traits, Allocator> &x,
                     basic_ostringstream<CharacterType, Traits, Allocator> &y) noexcept
    {
        x.swap(y);
    }

    template<class CharacterType, class Traits, class Allocator>
    inline void swap(basic_stringstream<CharacterType, Traits, Allocator> &x,
                     basic_stringstream<CharacterType, Traits, Allocator> &y) noexcept
    {
        x.swap(y);
    }
} // namespace SFTL
