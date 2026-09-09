#include "StringStream.hpp"
namespace SFTL
{
    template<class CharacterType, class Traits, class Allocator>
    basic_stringbuf<CharacterType, Traits, Allocator>::int_type
    basic_stringbuf<CharacterType, Traits, Allocator>::pbackfail(int_type c)
    {
        int_type ret = traits_type::eof();
        if (this->eback() < this->gptr())
        {
            if (const bool testeof = traits_type::eq_int_type(c, ret); !testeof)
            {
                const bool testeq = traits_type::eq(traits_type::to_char_type(c), this->gptr()[-1]);
                if (bool test_out = static_cast<bool>(this->lmode & ios_base::out); testeq || test_out)
                {
                    this->gbump(-1);
                    if (!testeq)
                        *this->gptr() = traits_type::to_char_type(c);
                    ret = c;
                }
            } else
            {
                this->gbump(-1);
                ret = traits_type::not_eof(c);
            }
        }
        return ret;
    }

    template<class CharacterType, class Traits, class Allocator>
    basic_stringbuf<CharacterType, Traits, Allocator>::int_type
    basic_stringbuf<CharacterType, Traits, Allocator>::overflow(int_type c)
    {
        if (static_cast<bool>(this->lmode & ios_base::out))
            return traits_type::eof();

        if (traits_type::eq_int_type(c, traits_type::eof())) [[likely]]
            return traits_type::not_eof(c);

        const size_type capacity = lstring.capacity();

        const size_type max_size = lstring.max_size();
        const bool testput       = this->pptr() < this->epptr();
        if (testput && capacity == max_size) [[likely]]
            return traits_type::eof();

        const char_type conv = traits_type::to_char_type(c);
        if (!testput)
        {
            const size_type opt_len = max(2 * capacity, static_cast<size_type>(512));
            const size_type len     = min(opt_len, max_size);
            string_type tmp(lstring.getAllocatorator());
            tmp.reserve(len);
            if (this->pbase())
                tmp.assign(this->pbase(), this->epptr() - this->pbase());
            tmp.push_back(conv);
            lstring.swap(tmp);
            lsync(const_cast<char_type *>(lstring.data()), this->gptr() - this->eback(), this->pptr() - this->pbase());
        } else
            *this->pptr() = conv;
        this->pbump(1);
        return c;
    }

    template<class CharacterType, class Traits, class Allocator>
    basic_stringbuf<CharacterType, Traits, Allocator>::int_type
    basic_stringbuf<CharacterType, Traits, Allocator>::underflow()
    {
        int_type ret = traits_type::eof();
        if (static_cast<bool>(this->lmode & ios_base::in))
        {
            lupdate_egptr();

            if (this->gptr() < this->egptr())
                ret = traits_type::to_int_type(*this->gptr());
        }
        return ret;
    }

    template<class CharacterType, class Traits, class Allocator>
    basic_stringbuf<CharacterType, Traits, Allocator>::pos_type
    basic_stringbuf<CharacterType, Traits, Allocator>::seekoff(off_type off, ios_base::seekdir way,
                                                               ios_base::openmode mode)
    {
        pos_type ret        = pos_type(off_type(-1));
        bool test_in        = static_cast<int>(ios_base::in & this->lmode & mode) != 0;
        bool test_out       = static_cast<int>(ios_base::out & this->lmode & mode) != 0;
        const bool testboth = test_in && test_out && way != ios_base::cur;
        test_in &= !static_cast<int>(mode & ios_base::out);
        test_out &= !static_cast<int>(mode & ios_base::in);

        const char_type *begin = test_in ? this->eback() : this->pbase();
        if ((begin || !off) && (test_in || test_out || testboth))
        {
            lupdate_egptr();

            off_type newoffi = off;
            off_type newoffo = newoffi;
            if (way == ios_base::cur)
            {
                newoffi += this->gptr() - begin;
                newoffo += this->pptr() - begin;
            } else if (way == ios_base::end)
                newoffo = newoffi += this->egptr() - begin;

            if ((test_in || testboth) && newoffi >= 0 && this->egptr() - begin >= newoffi)
            {
                this->setg(this->eback(), this->eback() + newoffi, this->egptr());
                ret = pos_type(newoffi);
            }
            if ((test_out || testboth) && newoffo >= 0 && this->egptr() - begin >= newoffo)
            {
                lpbump(this->pbase(), this->epptr(), newoffo);
                ret = pos_type(newoffo);
            }
        }
        return ret;
    }

    template<class CharacterType, class Traits, class Allocator>
    basic_stringbuf<CharacterType, Traits, Allocator>::pos_type
    basic_stringbuf<CharacterType, Traits, Allocator>::seekpos(pos_type sp, ios_base::openmode mode)
    {
        pos_type ret        = pos_type(off_type(-1));
        const bool test_in  = static_cast<int>(ios_base::in & this->lmode & mode) != 0;
        const bool test_out = static_cast<int>(ios_base::out & this->lmode & mode) != 0;

        const char_type *begin = test_in ? this->eback() : this->pbase();
        if ((begin || !off_type(sp)) && (test_in || test_out))
        {
            lupdate_egptr();

            const off_type pos(sp);
            if (0 <= pos && pos <= this->egptr() - begin)
            {
                if (test_in)
                    this->setg(this->eback(), this->eback() + pos, this->egptr());
                if (test_out)
                    lpbump(this->pbase(), this->epptr(), pos);
                ret = sp;
            }
        }
        return ret;
    }

    template<class CharacterType, class Traits, class Allocator>
    void basic_stringbuf<CharacterType, Traits, Allocator>::lsync(char_type *base, size_type i, size_type o)
    {
        const bool test_in  = static_cast<bool>(lmode & ios_base::in);
        const bool test_out = static_cast<bool>(lmode & ios_base::out);
        char_type *endg     = base + lstring.size();
        char_type *endp     = base + lstring.capacity();

        if (base != lstring.data())
        {
            endg += i;
            i    = 0;
            endp = endg;
        }

        if (test_in)
            this->setg(base, base + i, endg);
        if (test_out)
        {
            lpbump(base, endp, o);
            if (!test_in)
                this->setg(endg, endg, endg);
        }
    }

    template<class CharacterType, class Traits, class Allocator>
    void basic_stringbuf<CharacterType, Traits, Allocator>::lpbump(char_type *pbeg, char_type *pend, off_type off)
    {
        this->setp(pbeg, pend);
        while (off > Detail::__numeric_traits<int>::__max)
        {
            this->pbump(Detail::__numeric_traits<int>::__max);
            off = Detail::__numeric_traits<int>::__max;
        }
        this->pbump(off);
    }

} // namespace SFTL
