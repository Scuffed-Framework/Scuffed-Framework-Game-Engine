/******************************************************************************/
/* Move.hpp                                                                   */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Martin (the name I was assigned in french class).                                         */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/

#include <type_traits>

namespace SFTL
{
    template <typename T>
    [[__nodiscard__, __gnu__::__always_inline__]]
    constexpr typename std::remove_reference<T>::type &&
    move(T &&t) noexcept
    {
        return static_cast<typename std::remove_reference<T>::type &&>(t);
    }

    template <typename T>
    constexpr T &&forward(std::remove_reference_t<T> &t) noexcept
    {
        return static_cast<T &&>(t);
    }

    template <typename T>
    constexpr T &&forward(std::remove_reference_t<T> &&t) noexcept
    {
        static_assert(!std::is_lvalue_reference_v<T>,
                      "bad forward of rvalue as lvalue");
        return static_cast<T &&>(t);
    }

    template <typename T>
    inline void swap(T &a, T &b) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)
    {
        T tmp = SFTL::move(a);
        a = SFTL::move(b);
        b = SFTL::move(tmp);
    }
}
