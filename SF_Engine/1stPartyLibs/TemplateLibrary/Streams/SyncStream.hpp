#pragma once
#include "../Parallel/Mutex.hpp"
#include "StreamBuf.hpp"
#include "StringStream.hpp"

namespace SFTL
{
    template<typename CharacterType, typename Traits>
    class syncbuf_base : public basic_streambuf<CharacterType, Traits>
    {
    public:
        static bool *sget(basic_streambuf<CharacterType, Traits> *buf [[maybe_unused]]) noexcept
        {
            if (auto p = dynamic_cast<syncbuf_base *>(buf))
                return &p->Lemit_on_sync;
            return nullptr;
        }

    protected:
        syncbuf_base(basic_streambuf<CharacterType, Traits> *w = nullptr) : Lwrapped(w) {}

        basic_streambuf<CharacterType, Traits> *Lwrapped = nullptr;
        bool Lemit_on_sync                               = false;
        bool Lneeds_sync                                 = false;
    };

    template<typename CharacterType, typename Traits>
    inline basic_ostream<CharacterType, Traits> &emit_on_flush(basic_ostream<CharacterType, Traits> &os)
    {
        if (bool *flag = syncbuf_base<CharacterType, Traits>::sget(os.rdbuf()))
            *flag = true;
        return os;
    }

    template<typename CharacterType, typename Traits>
    inline basic_ostream<CharacterType, Traits> &noemit_on_flush(basic_ostream<CharacterType, Traits> &os)
    {
        if (bool *flag = syncbuf_base<CharacterType, Traits>::sget(os.rdbuf()))
            *flag = false;
        return os;
    }

    template<typename CharacterType, typename Traits>
    inline basic_ostream<CharacterType, Traits> &flush_emit(basic_ostream<CharacterType, Traits> &os)
    {

        if (bool *flag = syncbuf_base<CharacterType, Traits>::sget(os.rdbuf()))
        {
            struct Restore
            {
                ~Restore() { *Lflag = Lprev; }

                bool Lprev  = false;
                bool *Lflag = &Lprev;
            } restore;
            restore.Lprev = *flag;
            restore.Lflag = flag;
            *flag         = true;
        }

        os.flush();
        return os;
    }

    template<typename CharacterType, typename Traits, typename _Alloc>
    class basic_syncbuf : public syncbuf_base<CharacterType, Traits>
    {
    public:
        using char_type      = CharacterType;
        using int_type       = typename Traits::int_type;
        using pos_type       = typename Traits::pos_type;
        using off_type       = typename Traits::off_type;
        using traits_type    = Traits;
        using allocator_type = _Alloc;
        using streambuf_type = basic_streambuf<CharacterType, Traits>;

        basic_syncbuf() : basic_syncbuf(nullptr, allocator_type{}) {}

        explicit basic_syncbuf(streambuf_type *obuf) : basic_syncbuf(obuf, allocator_type{}) {}

        basic_syncbuf(streambuf_type *obuf, const allocator_type &alloc) :
            syncbuf_base<CharacterType, Traits>(obuf), Limpl(alloc), Lmtx(obuf)
        {
        }

        basic_syncbuf(basic_syncbuf &&other) noexcept :
            syncbuf_base<CharacterType, Traits>(other.Lwrapped), Limpl(move(other.Limpl)), Lmtx(move(other.Lmtx))
        {
            this->Lemit_on_sync = other.Lemit_on_sync;
            this->Lneeds_sync   = other.Lneeds_sync;
            other.Lwrapped      = nullptr;
        }

        ~basic_syncbuf() override { emit(); }

        basic_syncbuf &operator=(basic_syncbuf &&other) noexcept
        {
            emit();

            Limpl               = move(other.Limpl);
            this->Lemit_on_sync = other.Lemit_on_sync;
            this->Lneeds_sync   = other.Lneeds_sync;
            this->Lwrapped      = other.Lwrapped;
            other.Lwrapped      = nullptr;
            Lmtx                = move(other.Lmtx);

            return *this;
        }

        void swap(basic_syncbuf &other) noexcept
        {
            using _ATr = allocator_traits<_Alloc>;
            if constexpr (!_ATr::propagate_on_container_swap::value)
                glibcxx_assert(get_allocator() == other.get_allocator());

            swap(Limpl, other.Limpl);
            swap(this->Lemit_on_sync, other.Lemit_on_sync);
            swap(this->Lneeds_sync, other.Lneeds_sync);
            swap(this->Lwrapped, other.Lwrapped);
            swap(Lmtx, other.Lmtx);
        }

        bool emit()
        {
            if (!this->Lwrapped)
                return false;

            auto s = move(Limpl).str();

            const lock_guard<mutex> l(Lmtx);
            if (auto size = s.size())
            {
                auto n = this->Lwrapped->sputn(s.data(), size);
                if (n != size)
                {
                    s.erase(0, n);
                    Limpl.str(move(s));
                    return false;
                }
            }

            if (this->Lneeds_sync)
            {
                this->Lneeds_sync = false;
                if (this->Lwrapped->pubsync() != 0)
                    return false;
            }
            return true;
        }

        streambuf_type *get_wrapped() const noexcept { return this->Lwrapped; }

        allocator_type get_allocator() const noexcept { return Limpl.get_allocator(); }

        void set_emit_on_sync(bool b) noexcept { this->Lemit_on_sync = b; }

    protected:
        int sync() override
        {
            this->Lneeds_sync = true;
            if (this->Lemit_on_sync && !emit())
                return -1;
            return 0;
        }

        int_type overflow(int_type c) override
        {
            int_type eof = traits_type::eof();
            if (builtin_expect(!traits_type::eq_int_type(c, eof), true))
                return Limpl.sputc(c);
            return eof;
        }

        streamsize xsputn(const char_type *s, streamsize n) override { return Limpl.sputn(s, n); }

    private:
        basic_stringbuf<char_type, traits_type, allocator_type> Limpl;

        struct mutex
        {
            mutex(void *) {}
            void swap(mutex &&) noexcept {}
            void lock() {}
            void unlock() {}
            mutex(mutex &&)            = default;
            mutex &operator=(mutex &&) = default;
        };
        mutex Lmtx;
    };

    template<typename CharacterType, typename Traits, typename _Alloc>
    class basic_osyncstream : public basic_ostream<CharacterType, Traits>
    {
        using ostream_type = basic_ostream<CharacterType, Traits>;

    public:
        // Types:
        using char_type      = CharacterType;
        using traits_type    = Traits;
        using allocator_type = _Alloc;
        using int_type       = typename traits_type::int_type;
        using pos_type       = typename traits_type::pos_type;
        using off_type       = typename traits_type::off_type;
        using syncbuf_type   = basic_syncbuf<CharacterType, Traits, _Alloc>;
        using streambuf_type = typename syncbuf_type::streambuf_type;

    private:
        syncbuf_type Lsyncbuf;

    public:
        basic_osyncstream(streambuf_type *buf, const allocator_type &a) : Lsyncbuf(buf, a)
        {
            this->init(addressof(Lsyncbuf));
        }

        explicit basic_osyncstream(streambuf_type *buf) : Lsyncbuf(buf) { this->init(addressof(Lsyncbuf)); }

        basic_osyncstream(basic_ostream<char_type, traits_type> &os, const allocator_type &a) :
            basic_osyncstream(os.rdbuf(), a)
        {
            this->init(addressof(Lsyncbuf));
        }

        explicit basic_osyncstream(basic_ostream<char_type, traits_type> &os) : basic_osyncstream(os.rdbuf())
        {
            this->init(addressof(Lsyncbuf));
        }

        basic_osyncstream(basic_osyncstream &&rhs) noexcept : ostream_type(move(rhs)), Lsyncbuf(move(rhs.Lsyncbuf))
        {
            ostream_type::set_rdbuf(addressof(Lsyncbuf));
        }

        ~basic_osyncstream() = default;

        basic_osyncstream &operator=(basic_osyncstream &&) = default;

        syncbuf_type *rdbuf() const noexcept { return const_cast<syncbuf_type *>(&Lsyncbuf); }

        streambuf_type *get_wrapped() const noexcept { return Lsyncbuf.get_wrapped(); }

        void emit()
        {
            if (!Lsyncbuf.emit())
                this->setstate(ios_base::failbit);
        }
    };

    template<class CharacterType, class Traits, class Allocator>
    inline void swap(basic_syncbuf<CharacterType, Traits, Allocator> &x,
                     basic_syncbuf<CharacterType, Traits, Allocator> &y) noexcept
    {
        x.swap(y);
    }

    using syncbuf      = basic_syncbuf<char, char_traits<char>, allocator<char>>;
    using osyncstream  = basic_osyncstream<char, char_traits<char>, allocator<char>>;
    using wsyncbuf     = basic_syncbuf<wchar_t, char_traits<wchar_t>, allocator<wchar_t>>;
    using wosyncstream = basic_osyncstream<wchar_t, char_traits<wchar_t>, allocator<wchar_t>>;
} // namespace SFTL
