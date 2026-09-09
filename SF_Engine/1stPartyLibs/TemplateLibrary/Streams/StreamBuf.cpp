#include "StreamBuf.hpp"

namespace SFTL
{
    template<typename CharType, typename Traits>
    streamsize basic_streambuf<CharType, Traits>::xsgetn(char_type *ct, streamsize n)
    {
        streamsize ret = 0;
        while (ret < n)
        {
            if (const streamsize buf_len = this->egptr() - this->gptr())
            {
                const streamsize remaining = n - ret;
                const streamsize len       = min(buf_len, remaining);
                traits_type::copy(ct, this->gptr(), len);
                ret += len;
                ct += len;
                this->safe_gbump(len);
            }

            if (ret < n)
            {
                const int_type c = this->uflow();
                if (!traits_type::eq_int_type(c, traits_type::eof()))
                {
                    traits_type::assign(*ct++, traits_type::to_char_type(c));
                    ++ret;
                } else
                    break;
            }
        }
        return ret;
    }

    template<typename CharType, typename Traits>
    streamsize basic_streambuf<CharType, Traits>::xsputn(const char_type *ct, streamsize n)
    {
        streamsize ret = 0;
        while (ret < n)
        {
            if (const streamsize buf_len = this->epptr() - this->pptr())
            {
                const streamsize remaining = n - ret;
                const streamsize len       = min(buf_len, remaining);
                traits_type::copy(this->pptr(), ct, len);
                ret += len;
                ct += len;
                this->safe_pbump(len);
            }

            if (ret < n)
            {
                int_type c = this->overflow(traits_type::to_int_type(*ct));
                if (!traits_type::eq_int_type(c, traits_type::eof()))
                {
                    ++ret;
                    ++ct;
                } else
                    break;
            }
        }
        return ret;
    }

    template<typename CharType, typename Traits>
    streamsize copy_streambufs_eof(basic_streambuf<CharType, Traits> *sbin, basic_streambuf<CharType, Traits> *ctbout,
                                   bool &eof)
    {
        streamsize ret              = 0;
        eof                         = true;
        typename Traits::int_type a = sbin->sgetc();
        while (!Traits::eq_int_type(a, Traits::eof()))
        {
            a = ctbout->sputc(Traits::to_char_type(a));
            if (Traits::eq_int_type(a, Traits::eof()))
            {
                eof = false;
                break;
            }
            ++ret;
            a = sbin->snextc();
        }
        return ret;
    }

} // namespace SFTL
