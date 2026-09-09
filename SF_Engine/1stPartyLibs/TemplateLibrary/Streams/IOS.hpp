#pragma once

#include "../Atomic.hpp"
#include "../Locale.hpp"

namespace SFTL
{
    template<typename CharT>
    class ctype;
    template<typename CharT, typename OutputIt>
    class num_put;
    template<typename CharT, typename InputIt>
    class num_get;

    class ios_base;

    template<typename CharT, typename Traits = char_traits<CharT>>
    class basic_ios;

    template<typename CharT, typename Traits = char_traits<CharT>>
    class basic_streambuf;

    template<typename CharT, typename Traits = char_traits<CharT>>
    class basic_istream;

    template<typename CharT, typename Traits = char_traits<CharT>>
    class basic_ostream;
    
    typedef basic_ostream<char> ostream;
    typedef basic_ostream<char> wostream;
    typedef basic_istream<char> istream;
    typedef basic_istream<char> wistream;

    enum class FormatFlags
    {
        s_boolalpha             = 1L << 0,
        s_dec                   = 1L << 1,
        s_fixed                 = 1L << 2,
        s_hex                   = 1L << 3,
        s_internal              = 1L << 4,
        s_left                  = 1L << 5,
        s_oct                   = 1L << 6,
        s_right                 = 1L << 7,
        s_scientific            = 1L << 8,
        s_showbase              = 1L << 9,
        s_showpoint             = 1L << 10,
        s_showpos               = 1L << 11,
        s_skipws                = 1L << 12,
        s_unitbuf               = 1L << 13,
        s_uppercase             = 1L << 14,
        s_adjustfield           = s_left | s_right | s_internal,
        s_basefield             = s_dec | s_oct | s_hex,
        s_floatfield            = s_scientific | s_fixed,
        sFormatFlags_end UNUSED = 1L << 16,
        sFormatFlags_max UNUSED = __INT_MAX__,
        sFormatFlags_min UNUSED = ~__INT_MAX__
    };

    [[nodiscard]] constexpr inline FormatFlags operator&(FormatFlags A, FormatFlags B)
    {
        return FormatFlags(static_cast<int>(A) & static_cast<int>(B));
    }

    [[nodiscard]] constexpr inline FormatFlags operator|(FormatFlags A, FormatFlags B)
    {
        return FormatFlags(static_cast<int>(A) | static_cast<int>(B));
    }

    [[nodiscard]] constexpr inline FormatFlags operator^(FormatFlags A, FormatFlags B)
    {
        return FormatFlags(static_cast<int>(A) ^ static_cast<int>(B));
    }

    [[nodiscard]] constexpr inline FormatFlags operator~(FormatFlags A) { return FormatFlags(~static_cast<int>(A)); }

    constexpr FormatFlags &operator|=(FormatFlags &A, const FormatFlags B) { return A = A | B; }
    constexpr FormatFlags &operator&=(FormatFlags &A, const FormatFlags B) { return A = A & B; }
    constexpr FormatFlags &operator^=(FormatFlags &A, const FormatFlags B) { return A = A ^ B; }


    enum class OpenMode
    {
        s_app                = 1L << 0,
        s_ate                = 1L << 1,
        s_bin                = 1L << 2,
        s_in                 = 1L << 3,
        s_out                = 1L << 4,
        s_trunc              = 1L << 5,
        s_noreplace UNUSED   = 1L << 6,
        sOpenMode_end UNUSED = 1L << 16,
        sOpenMode_max UNUSED = __INT_MAX__,
        sOpenMode_min UNUSED = ~__INT_MAX__
    };


    [[nodiscard]] constexpr inline OpenMode operator&(OpenMode A, OpenMode B)
    {
        return OpenMode(static_cast<int>(A) & static_cast<int>(B));
    }

    [[nodiscard]] constexpr inline OpenMode operator|(OpenMode A, OpenMode B)
    {
        return OpenMode(static_cast<int>(A) | static_cast<int>(B));
    }

    [[nodiscard]] constexpr inline OpenMode operator^(OpenMode A, OpenMode B)
    {
        return OpenMode(static_cast<int>(A) ^ static_cast<int>(B));
    }

    [[nodiscard]] constexpr OpenMode operator~(OpenMode A) { return OpenMode(~static_cast<int>(A)); }
    constexpr const OpenMode &operator|=(OpenMode &A, OpenMode B) { return A = A | B; }
    constexpr const OpenMode &operator&=(OpenMode &A, OpenMode B) { return A = A & B; }
    constexpr const OpenMode &operator^=(OpenMode &A, OpenMode B) { return A = A ^ B; }


    enum class iostate_impl
    {
        s_goodbit           = 0,
        s_badbit            = 1L << 0,
        s_eofbit            = 1L << 1,
        s_failbit           = 1L << 2,
        siostate_end UNUSED = 1L << 16,
        siostate_max UNUSED = __INT_MAX__,
        siostate_min UNUSED = ~__INT_MAX__
    };

    [[nodiscard]] constexpr inline iostate_impl operator&(iostate_impl A, iostate_impl B)
    {
        return iostate_impl(static_cast<int>(A) & static_cast<int>(B));
    }

    [[nodiscard]] constexpr inline iostate_impl operator|(iostate_impl A, iostate_impl B)
    {
        return iostate_impl(static_cast<int>(A) | static_cast<int>(B));
    }

    [[nodiscard]] constexpr inline iostate_impl operator^(iostate_impl A, iostate_impl B)
    {
        return iostate_impl(static_cast<int>(A) ^ static_cast<int>(B));
    }

    [[nodiscard]] constexpr iostate_impl operator~(iostate_impl A) { return iostate_impl(~static_cast<int>(A)); }
    constexpr const iostate_impl &operator|=(iostate_impl &A, iostate_impl B) { return A = A | B; }
    constexpr const iostate_impl &operator&=(iostate_impl &A, iostate_impl B) { return A = A & B; }
    constexpr iostate_impl operator^=(iostate_impl &A, iostate_impl B) { return A = A ^ B; }


    enum SeekDir
    {
        s_beg              = 0,
        s_cur              = 1,
        s_end              = 2,
        seekdir_end UNUSED = 1L << 16
    };

    class ios_base
    {
    public:
        typedef FormatFlags fmtflags;

        static constexpr auto boolalpha   = FormatFlags::s_boolalpha;
        static constexpr auto dec         = FormatFlags::s_dec;
        static constexpr auto fixed       = FormatFlags::s_fixed;
        static constexpr auto hex         = FormatFlags::s_hex;
        static constexpr auto internal    = FormatFlags::s_internal;
        static constexpr auto left        = FormatFlags::s_left;
        static constexpr auto oct         = FormatFlags::s_oct;
        static constexpr auto right       = FormatFlags::s_right;
        static constexpr auto scientific  = FormatFlags::s_scientific;
        static constexpr auto showbase    = FormatFlags::s_showbase;
        static constexpr auto showpoint   = FormatFlags::s_showpoint;
        static constexpr auto showpos     = FormatFlags::s_showpos;
        static constexpr auto skipws      = FormatFlags::s_skipws;
        static constexpr auto unitbuf     = FormatFlags::s_unitbuf;
        static constexpr auto uppercase   = FormatFlags::s_uppercase;
        static constexpr auto adjustfield = FormatFlags::s_adjustfield;
        static constexpr auto basefield   = FormatFlags::s_basefield;
        static constexpr auto floatfield  = FormatFlags::s_floatfield;


        typedef iostate_impl iostate;

        static constexpr auto badbit  = iostate_impl::s_badbit;
        static constexpr auto eofbit  = iostate_impl::s_eofbit;
        static constexpr auto failbit = iostate_impl::s_failbit;
        static constexpr auto goodbit = iostate_impl::s_goodbit;

        typedef OpenMode openmode;

        static constexpr auto app       = OpenMode::s_app;
        static constexpr auto ate       = OpenMode::s_ate;
        static constexpr auto binary    = OpenMode::s_bin;
        static constexpr auto in        = OpenMode::s_in;
        static constexpr auto out       = OpenMode::s_out;
        static constexpr auto trunc     = OpenMode::s_trunc;
        static constexpr auto noreplace = OpenMode::s_noreplace;

        typedef SeekDir seekdir;

        static constexpr seekdir beg = s_beg;
        static constexpr seekdir cur = s_cur;
        static constexpr seekdir end = s_end;

        enum event
        {
            erase_event,
            imbue_event,
            copyfmt_event
        };


        typedef void (*event_callback)(event ev, ios_base &B, int i);
        void register_callback(event_callback func, int index);

    protected:
        streamsize lprecision;
        streamsize lwidth;
        fmtflags lflags;
        iostate lexception;
        iostate lstreambuf_state;

        struct CallbackList
        {
            CallbackList *lnext;
            event_callback lfn;
            int lindex;
            atomic_word lrefcount; // 0 means one reference.

            CallbackList(event_callback func, int index, CallbackList *callback) :
                lnext(callback), lfn(func), lindex(index), lrefcount(0)
            {
            }

            void add_reference() { atomic_add_dispatch(&lrefcount, 1); }

            int remove_reference()
            {
                int res = exchange_and_add_dispatch(&lrefcount, -1);
                return res;
            }
        };

        CallbackList *callbacks;

        void call_callbacks(event evv) noexcept;
        void dispose_callbacks(void) noexcept;

        struct Words
        {
            void *lpword;
            long liword;
            Words() : lpword(nullptr), liword(0) {}
        };

        Words word_zero;
        enum
        {
            s_local_word_size = 8
        };

        Words local_word[s_local_word_size];

        int lword_size;
        Words *lword;

        Words &growWords(int index, bool iword);
        locale ioslocale;

        void init() noexcept;

    public:
        class Init
        {
            friend class ios_base;

        public:
            Init();
            ~Init();

            Init(const Init &)            = default;
            Init &operator=(const Init &) = default;

        private:
            static atomic_word s_refcount;
            static bool s_synced_with_stdio;
        };

        [[nodiscard]] fmtflags flags() const { return lflags; }
        fmtflags flags(fmtflags formatflags)
        {
            fmtflags old = lflags;
            lflags       = formatflags;
            return old;
        }

        fmtflags setf(fmtflags formatflags)
        {
            fmtflags old = lflags;
            lflags |= formatflags;
            return old;
        }

        fmtflags setf(fmtflags formatflags, fmtflags mask)
        {
            const fmtflags old = lflags;
            lflags &= ~mask;
            lflags |= (formatflags & mask);
            return old;
        }

        void unsetf(fmtflags mask) { lflags &= ~mask; }

        [[nodiscard]] streamsize precision() const { return lprecision; }
        streamsize precision(streamsize precision)
        {
            streamsize old = lprecision;
            lprecision     = precision;
            return old;
        }

        [[nodiscard]] streamsize width() const { return lwidth; }

        streamsize width(streamsize lwide)
        {
            streamsize old = lwidth;
            lwidth         = lwide;
            return old;
        }

        static bool sync_with_stdio(bool synchronize = true);

        locale imbue(const locale &loc) noexcept;
        [[nodiscard]] locale getloc() const { return ioslocale; }
        [[nodiscard]] const locale &lgetloc() const { return ioslocale; }

        static int xalloc() noexcept;
        long &iword(int i)
        {
            Words &f = (static_cast<unsigned>(i) < static_cast<unsigned>(lword_size)) ? lword[i] : growWords(i, true);
            return f.liword;
        }

        void *&pword(int i)
        {
            Words &f = (static_cast<unsigned>(i) < static_cast<unsigned>(lword_size)) ? lword[i] : growWords(i, false);
            return f.lpword;
        }


        virtual ~ios_base();

    protected:
        ios_base() noexcept;

    public:
        ios_base(const ios_base &) = delete;

        ios_base &operator=(const ios_base &) = delete;

    protected:
        void move(ios_base &) noexcept;

        void swap(ios_base &righthandside) noexcept;
    };

    inline ios_base &boolalpha(ios_base &base)
    {
        base.setf(ios_base::boolalpha);
        return base;
    }

    inline ios_base &noboolalpha(ios_base &base)
    {
        base.unsetf(ios_base::boolalpha);
        return base;
    }

    inline ios_base &showbase(ios_base &base)
    {
        base.setf(ios_base::showbase);
        return base;
    }

    inline ios_base &noshowbase(ios_base &base)
    {
        base.unsetf(ios_base::showbase);
        return base;
    }

    inline ios_base &showpoint(ios_base &base)
    {
        base.setf(ios_base::showpoint);
        return base;
    }

    inline ios_base &noshowpoint(ios_base &base)
    {
        base.unsetf(ios_base::showpoint);
        return base;
    }

    inline ios_base &showpos(ios_base &base)
    {
        base.setf(ios_base::showpos);
        return base;
    }

    inline ios_base &noshowpos(ios_base &base)
    {
        base.unsetf(ios_base::showpos);
        return base;
    }

    inline ios_base &skipws(ios_base &base)
    {
        base.setf(ios_base::skipws);
        return base;
    }

    inline ios_base &noskipws(ios_base &base)
    {
        base.unsetf(ios_base::skipws);
        return base;
    }

    inline ios_base &uppercase(ios_base &base)
    {
        base.setf(ios_base::uppercase);
        return base;
    }

    inline ios_base &nouppercase(ios_base &base)
    {
        base.unsetf(ios_base::uppercase);
        return base;
    }

    inline ios_base &unitbuf(ios_base &base)
    {
        base.setf(ios_base::unitbuf);
        return base;
    }

    inline ios_base &nounitbuf(ios_base &base)
    {
        base.unsetf(ios_base::unitbuf);
        return base;
    }

    inline ios_base &internal(ios_base &base)
    {
        base.setf(ios_base::internal, ios_base::adjustfield);
        return base;
    }

    inline ios_base &left(ios_base &base)
    {
        base.setf(ios_base::left, ios_base::adjustfield);
        return base;
    }

    inline ios_base &right(ios_base &base)
    {
        base.setf(ios_base::right, ios_base::adjustfield);
        return base;
    }

    inline ios_base &dec(ios_base &base)
    {
        base.setf(ios_base::dec, ios_base::basefield);
        return base;
    }

    inline ios_base &hex(ios_base &base)
    {
        base.setf(ios_base::hex, ios_base::basefield);
        return base;
    }

    inline ios_base &oct(ios_base &base)
    {
        base.setf(ios_base::oct, ios_base::basefield);
        return base;
    }

    inline ios_base &fixed(ios_base &base)
    {
        base.setf(ios_base::fixed, ios_base::floatfield);
        return base;
    }

    inline ios_base &scientific(ios_base &base)
    {
        base.setf(ios_base::scientific, ios_base::floatfield);
        return base;
    }


    inline ios_base &hexfloat(ios_base &base)
    {
        base.setf(ios_base::fixed | ios_base::scientific, ios_base::floatfield);
        return base;
    }

    inline ios_base &defaultfloat(ios_base &base)
    {
        base.unsetf(ios_base::floatfield);
        return base;
    }
    template<typename Type>
    concept derived_from_ios_base =
            is_class_v<Type> && (!is_same_v<Type, ios_base>) && requires(Type *t, ios_base *v) { v = t; };


    template<typename Facet>
    const Facet &check_facet(const Facet *f)
    {
        if (!f)
            return *static_cast<const Facet *>(nullptr); // or handle appropriately
        return *f;
    }

    template<typename CharT, typename Traits = char_traits<CharT>>
    class istreambuf_iterator
    {
    public:
        using iterator_category = input_iterator_tag;
        using value_type        = CharT;
        using difference_type   = typename Traits::off_type;
        using pointer           = const CharT *;
        using reference         = const CharT;
        using char_type         = CharT;
        using traits_type       = Traits;
        using streambuf_type    = basic_streambuf<CharT, Traits>;
        using istream_type      = basic_istream<CharT, Traits>;

        constexpr istreambuf_iterator() noexcept : _sbuf(nullptr) {}

        istreambuf_iterator(istream_type &stream) noexcept : _sbuf(stream.rdbuf()) {}

        istreambuf_iterator(streambuf_type *sbuf) noexcept : _sbuf(sbuf) {}

        CharT operator*() const { return static_cast<CharT>(_sbuf->sgetc()); }

        istreambuf_iterator &operator++()
        {
            _sbuf->sbumpc();
            return *this;
        }

        class proxy
        {
            CharT _keep;
            streambuf_type *_sbuf;

        public:
            proxy(CharT c, streambuf_type *sbuf) : _keep(c), _sbuf(sbuf) {}
            CharT operator*() const { return _keep; }
        };

        proxy operator++(int)
        {
            CharT c = static_cast<CharT>(_sbuf->sgetc());
            _sbuf->sbumpc();
            return proxy(c, _sbuf);
        }

        bool equal(const istreambuf_iterator &other) const
        {
            bool a_eof = (_sbuf == nullptr || _sbuf->sgetc() == traits_type::eof());
            bool b_eof = (other._sbuf == nullptr || other._sbuf->sgetc() == traits_type::eof());
            if (a_eof && b_eof)
                return true;
            return _sbuf == other._sbuf;
        }

    private:
        streambuf_type *_sbuf;
    };

    template<typename CharT, typename Traits>
    inline bool operator==(const istreambuf_iterator<CharT, Traits> &a, const istreambuf_iterator<CharT, Traits> &b)
    {
        return a.equal(b);
    }

    template<typename CharT, typename Traits>
    inline bool operator!=(const istreambuf_iterator<CharT, Traits> &a, const istreambuf_iterator<CharT, Traits> &b)
    {
        return !a.equal(b);
    }

    template<typename CharT, typename Traits = char_traits<CharT>>
    class ostreambuf_iterator
    {
    public:
        using iterator_category = output_iterator_tag;
        using value_type        = void;
        using difference_type   = void;
        using pointer           = void;
        using reference         = void;
        using char_type         = CharT;
        using traits_type       = Traits;
        using streambuf_type    = basic_streambuf<CharT, Traits>;
        using ostream_type      = basic_ostream<CharT, Traits>;

        ostreambuf_iterator(ostream_type &stream) noexcept : _sbuf(stream.rdbuf()) {}

        ostreambuf_iterator(streambuf_type *sbuf) noexcept : _sbuf(sbuf) {}

        ostreambuf_iterator &operator=(CharT c)
        {
            if (_sbuf && traits_type::eq_int_type(_sbuf->sputc(c), traits_type::eof()))
            {
                _sbuf = nullptr;
            }
            return *this;
        }

        ostreambuf_iterator &operator*() { return *this; }
        ostreambuf_iterator &operator++() { return *this; }
        ostreambuf_iterator operator++(int) { return *this; }

        bool failed() const noexcept { return _sbuf == nullptr; }

    private:
        streambuf_type *_sbuf;
    };

    template<typename CharacterType, typename Traits>
    class basic_ios : public ios_base
    {
        static_assert(is_same_v<CharacterType, typename Traits::char_type>);

    public:
        typedef CharacterType char_type;
        typedef typename Traits::int_type int_type;
        typedef typename Traits::pos_type pos_type;
        typedef typename Traits::off_type off_type;
        typedef Traits traits_type;

        typedef ctype<CharacterType> ctype_type;
        typedef ::SFTL::num_put<CharacterType, ostreambuf_iterator<CharacterType, Traits>> num_put_type;
        typedef ::SFTL::num_get<CharacterType, ::SFTL::istreambuf_iterator<CharacterType, Traits>> num_get_type;

    protected:
        basic_ostream<CharacterType, Traits> *ltie;
        mutable char_type lfill;
        mutable bool fill_initialize;

        basic_streambuf<CharacterType, Traits> *lstreambuf;
        iostate lstreambuf_state;

        const ctype_type *lctype;
        const num_put_type *lnum_put;
        const num_get_type *lnum_get;

    public:
        [[nodiscard]]
        explicit operator bool() const
        {
            return !this->fail();
        }

        [[nodiscard]]
        bool operator!() const
        {
            return this->fail();
        }

        [[nodiscard]]
        iostate rdstate() const
        {
            return lstreambuf_state;
        }

        void clear(iostate state = goodbit);

        void setstate(iostate state) { this->clear(this->rdstate() | state); }

        void lsetstate(iostate state)
        {
            lstreambuf_state = static_cast<iostate_impl>(lstreambuf_state | state);
            if (this->exceptions() & state)
            {
                throw;
            }
        }

        [[nodiscard]]
        bool good() const
        {
            return this->rdstate() == goodbit;
        }

        [[nodiscard]]
        bool eof() const
        {
            return (this->rdstate() & eofbit) != 0;
        }

        [[nodiscard]]
        bool fail() const
        {
            return (this->rdstate() & (badbit | failbit)) != 0;
        }

        [[nodiscard]]
        bool bad() const
        {
            return (this->rdstate() & badbit) != 0;
        }

        [[nodiscard]]
        iostate exceptions() const
        {
            return lexception;
        }

        void exceptions(iostate except)
        {
            lexception = except;
            this->clear(lstreambuf_state);
        }

        explicit basic_ios(basic_streambuf<CharacterType, Traits> *sb) :
            ios_base(), ltie(nullptr), lfill(), fill_initialize(false), lstreambuf(nullptr), lstreambuf_state(goodbit),
            lctype(nullptr), lnum_put(nullptr), lnum_get(nullptr)
        {
            this->init(sb);
        }

        ~basic_ios() override {}

        [[nodiscard]] const ctype_type *get_ctype() const { return lctype; }

        [[nodiscard]]
        basic_ostream<CharacterType, Traits> *tie() const
        {
            return ltie;
        }

        basic_ostream<CharacterType, Traits> *tie(basic_ostream<CharacterType, Traits> *tie_str)
        {
            basic_ostream<CharacterType, Traits> *old = ltie;
            ltie                                      = tie_str;
            return old;
        }

        [[nodiscard]]
        basic_streambuf<CharacterType, Traits> *rdbuf() const
        {
            return lstreambuf;
        }

        basic_streambuf<CharacterType, Traits> *rdbuf(basic_streambuf<CharacterType, Traits> *sb);
        basic_ios &copyfmt(const basic_ios &rhs);

        [[nodiscard]]
        char_type fill() const
        {
            if (!fill_initialize, false) [[likely]]
                return this->widen(' ');
            return lfill;
        }

        char_type fill(char_type ch)
        {
            char_type old   = lfill;
            lfill           = ch;
            fill_initialize = true;
            return old;
        }

        locale imbue(const locale &loc) noexcept;
        char narrow(char_type c, char d) const { return check_facet(lctype).narrow(c, d); }
        char_type widen(char c) const { return check_facet(lctype).widen(c); }

    protected:
        basic_ios() :
            ios_base(), ltie(nullptr), lfill(char_type()), fill_initialize(false), lstreambuf(nullptr),
            lstreambuf_state(goodbit), lctype(nullptr), lnum_put(nullptr), lnum_get(nullptr)
        {
        }

        void init(basic_streambuf<CharacterType, Traits> *sb);

        basic_ios(const basic_ios &)            = delete;
        basic_ios &operator=(const basic_ios &) = delete;

        void move(basic_ios &rhs)
        {
            ios_base::move(rhs);
            lcache_locale(ioslocale);
            this->tie(rhs.tie(nullptr));
            lfill            = rhs.lfill;
            fill_initialize  = rhs.fill_initialize;
            lstreambuf       = nullptr;
            lstreambuf_state = rhs.lstreambuf_state;
        }

        void move(basic_ios &&rhs) { this->move(rhs); }

        void swap(basic_ios &rhs) noexcept
        {
            ios_base::swap(rhs);
            lcache_locale(ioslocale);
            rhs.lcache_locale(rhs.ioslocale);
            SFTL::swap(ltie, rhs.ltie);
            SFTL::swap(lfill, rhs.lfill);
            SFTL::swap(fill_initialize, rhs.fill_initialize);
            SFTL::swap(lstreambuf, rhs.lstreambuf);
            SFTL::swap(lstreambuf_state, rhs.lstreambuf_state);
        }

        void set_rdbuf(basic_streambuf<CharacterType, Traits> *sb) { lstreambuf = sb; }

        void lcache_locale(const locale &loc);
    };

} // namespace SFTL
