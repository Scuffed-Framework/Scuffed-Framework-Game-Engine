#include "IOS.hpp"
#include "StreamBuf.hpp"

namespace SFTL
{
    template<typename CharType, typename Traits>
    void basic_ios<CharType, Traits>::clear(iostate state)
    {
        if (this->rdbuf())
            lstreambuf_state = state;
        else
            lstreambuf_state = state | badbit;
    }

    template<typename CharType, typename Traits>
    basic_streambuf<CharType, Traits> *basic_ios<CharType, Traits>::rdbuf(basic_streambuf<CharType, Traits> *sb)
    {
        basic_streambuf<CharType, Traits> *old = lstreambuf;
        lstreambuf                             = sb;
        this->clear();
        return old;
    }

    template<typename CharType, typename Traits>
    basic_ios<CharType, Traits> &basic_ios<CharType, Traits>::copyfmt(const basic_ios &rhs)
    {
        if (this != addressof(rhs))
        {
            Words *words     = (rhs.lword_size <= s_local_word_size) ? local_word : new Words[rhs.lword_size];
            CallbackList *cb = rhs.callbacks;
            if (cb)
                cb->add_reference();
            call_callbacks(erase_event);
            if (lword != local_word)
            {
                delete[] lword;
                lword = nullptr;
            }
            dispose_callbacks();
            callbacks = cb;
            for (int i = 0; i < rhs.lword_size; ++i)
                words[i] = rhs.lword[i];
            lword      = words;
            lword_size = rhs.lword_size;

            this->flags(rhs.flags());
            this->width(rhs.width());
            this->precision(rhs.precision());
            this->tie(rhs.tie());
            this->fill(rhs.fill());
            ioslocale = rhs.getloc();
            lcache_locale(ioslocale);

            call_callbacks(copyfmt_event);
            this->exceptions(rhs.exceptions());
        }
        return *this;
    }

    template<typename CharType, typename Traits>
    locale basic_ios<CharType, Traits>::imbue(const locale &loc) noexcept
    {
        locale old(this->getloc());
        ios_base::imbue(loc);
        lcache_locale(loc);
        if (this->rdbuf() != nullptr)
            this->rdbuf()->pubimbue(loc);
        return old;
    }

    template<typename CharType, typename Traits>
    void basic_ios<CharType, Traits>::init(basic_streambuf<CharType, Traits> *sb)
    {
        ios_base::init();

        lcache_locale(ioslocale);
        if (lctype)
        {
            lfill           = lctype->widen(' ');
            fill_initialize = true;
        } else
            fill_initialize = false;

        ltie             = 0;
        lexception       = goodbit;
        lstreambuf       = sb;
        lstreambuf_state = sb ? goodbit : badbit;
    }

    template<typename CharType, typename Traits>
    void basic_ios<CharType, Traits>::lcache_locale(const locale &loc)
    {
        lctype   = try_use_facet<ctype_type>(loc);
        lnum_put = try_use_facet<num_put_type>(loc);
        lnum_get = try_use_facet<num_get_type>(loc);
    }
} // namespace SFTL
