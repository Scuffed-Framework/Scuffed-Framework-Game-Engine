#pragma once

#include "../Types.hpp"
namespace SFTL
{
    typedef long long streamoff;
    typedef ptrdiff_t streamsize;

    template <typename T>
    class fpos
    {
    private:
        streamoff Moff;
        T Mstate;

    public:
        fpos() : Moff(0), Mstate() {}

        fpos(streamoff off)
            : Moff(off), Mstate() {}

        fpos(const fpos &) = default;
        fpos &operator=(const fpos &) = default;
        ~fpos() = default;

        /// Convert to streamoff.
        operator streamoff() const { return Moff; }

        /// Remember the value of @a st.
        void
        state(T st)
        {
            Mstate = st;
        }

        /// Return the last set value of @a st.
        T
        state() const
        {
            return Mstate;
        }

        fpos & operator+=(streamoff off)
        {
            Moff += off;
            return *this;
        }

        fpos & operator-=(streamoff off)
        {
            Moff -= off;
            return *this;
        }

        fpos operator+(streamoff off) const
        {
            fpos pos(*this);
            pos += off;
            return pos;
        }

        fpos operator-(streamoff off) const
        {
            fpos pos(*this);
            pos -= off;
            return pos;
        }

        streamoff operator-(const fpos &other) const
        {
            return Moff - other.Moff;
        }
    };

    template <typename T>
    inline bool operator==(const fpos<T> &lhs, const fpos<T> &rhs)
    {
        return streamoff(lhs) == streamoff(rhs);
    }

    template <typename T>
    inline bool operator!=(const fpos<T> &lhs, const fpos<T> &rhs)
    {
        return streamoff(lhs) != streamoff(rhs);
    }

    typedef fpos<mbstate_type> streampos;
    typedef fpos<mbstate_type> wstreampos;
    typedef fpos<mbstate_type> u8streampos;
    typedef fpos<mbstate_type> u16streampos;
    typedef fpos<mbstate_type> u32streampos;
} // namespace SFTL
