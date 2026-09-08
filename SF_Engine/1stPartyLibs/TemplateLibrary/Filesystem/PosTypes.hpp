/******************************************************************************/
/* PosTypes.hpp                                                               */
/******************************************************************************/
/*            This file is part of                                            */
/*            Scuffed Framework Standard Template Library                     */
/******************************************************************************/
#pragma once

#include "../Types.hpp"

namespace SFTL
{
    typedef long long StreamOffset;
    typedef ptrdiff_t StreamSize;

    typedef StreamOffset off_type;
    typedef mbstate_type state_type;

    template<typename T>
    class fpos
    {
    private:
        StreamOffset Moff;
        T Mstate;

    public:
        fpos() : Moff(0), Mstate() {}

        explicit fpos(StreamOffset off) : Moff(off), Mstate() {}

        fpos(const fpos &)            = default;
        fpos &operator=(const fpos &) = default;
        ~fpos()                       = default;

        explicit operator StreamOffset() const { return Moff; }

        void state(T st) { Mstate = st; }

        T state() const { return Mstate; }

        fpos &operator+=(StreamOffset off)
        {
            Moff += off;
            return *this;
        }

        fpos &operator-=(StreamOffset off)
        {
            Moff -= off;
            return *this;
        }

        fpos operator+(StreamOffset off) const
        {
            fpos pos(*this);
            pos += off;
            return pos;
        }

        fpos operator-(StreamOffset off) const
        {
            fpos pos(*this);
            pos -= off;
            return pos;
        }

        StreamOffset operator-(const fpos &other) const { return Moff - other.Moff; }
    };

    template<typename T>
    bool operator==(const fpos<T> &lhs, const fpos<T> &rhs)
    {
        return StreamOffset(lhs) == StreamOffset(rhs);
    }

    template<typename T>
    bool operator!=(const fpos<T> &lhs, const fpos<T> &rhs)
    {
        return StreamOffset(lhs) != StreamOffset(rhs);
    }

    typedef fpos<state_type> pos_type;

    // Convenience aliases
    typedef pos_type StreamPosition;
    typedef pos_type wStreamPosition;
    typedef pos_type u8StreamPosition;
    typedef pos_type u16StreamPosition;
    typedef pos_type u32StreamPosition;
} // namespace SFTL
