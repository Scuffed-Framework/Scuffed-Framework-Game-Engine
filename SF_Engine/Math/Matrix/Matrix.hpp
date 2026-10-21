#pragma once

#include <cstddef>
#include <functional>
#include <glm/glm.hpp>
#include "../Vectors/Vector.hpp"

namespace SF::Engine
{
// ---------------------------------------------------------------------------
// Type aliases
//   SF_DEFINE_MAT_TYPES(C, R)  -> Mat<C>x<R>, DMat<C>x<R>   (C columns, R rows)
//   SF_DEFINE_SQUARE_MAT_TYPES(N) -> the above for NxN, plus Mat<N>, DMat<N>
// ---------------------------------------------------------------------------
#define SF_DEFINE_MAT_TYPES(C, R)                                                                                      \
    using Mat##C##x##R  = glm::mat<C, R, float>;                                                                       \
    using DMat##C##x##R = glm::mat<C, R, double>;

#define SF_DEFINE_SQUARE_MAT_TYPES(N)                                                                                  \
    SF_DEFINE_MAT_TYPES(N, N)                                                                                          \
    using Mat##N  = Mat##N##x##N;                                                                                      \
    using DMat##N = DMat##N##x##N;

    SF_DEFINE_SQUARE_MAT_TYPES(2)
    SF_DEFINE_SQUARE_MAT_TYPES(3)
    SF_DEFINE_SQUARE_MAT_TYPES(4)

    SF_DEFINE_MAT_TYPES(2, 3)
    SF_DEFINE_MAT_TYPES(2, 4)
    SF_DEFINE_MAT_TYPES(3, 2)
    SF_DEFINE_MAT_TYPES(3, 4)
    SF_DEFINE_MAT_TYPES(4, 2)
    SF_DEFINE_MAT_TYPES(4, 3)

// ---------------------------------------------------------------------------
// Mixed float/double operators (square matrices). GLM has explicit converting
// constructors between mat types, so we promote to double and operate there.
// Requires the Vec types from your vector header for the mat*vec overloads.
// ---------------------------------------------------------------------------
#define SF_DEFINE_MIXED_MAT_OP(OP, A, B, R)                                                                            \
    inline R operator OP(const A &lhs, const B &rhs) noexcept { return R(lhs) OP R(rhs); }

#define SF_DEFINE_MIXED_MAT_OP_SYM(OP, A, B, R)                                                                        \
    SF_DEFINE_MIXED_MAT_OP(OP, A, B, R)                                                                                \
    SF_DEFINE_MIXED_MAT_OP(OP, B, A, R)

// +, -, and * (matrix multiply) between Mat<N> and DMat<N> -> DMat<N>
#define SF_DEFINE_MIXED_MAT_ARITH(N)                                                                                   \
    SF_DEFINE_MIXED_MAT_OP_SYM(+, Mat##N, DMat##N, DMat##N)                                                            \
    SF_DEFINE_MIXED_MAT_OP_SYM(-, Mat##N, DMat##N, DMat##N)                                                            \
    SF_DEFINE_MIXED_MAT_OP_SYM(*, Mat##N, DMat##N, DMat##N)

// Mat<N> * DVec<N> and DMat<N> * Vec<N> -> DVec<N>
#define SF_DEFINE_MIXED_MAT_VEC(N)                                                                                     \
    inline DVec##N operator*(const Mat##N &m, const DVec##N &v) noexcept { return DMat##N(m) * v; }                    \
    inline DVec##N operator*(const DMat##N &m, const Vec##N &v) noexcept { return m * DVec##N(v); }

    SF_DEFINE_MIXED_MAT_ARITH(2)
    SF_DEFINE_MIXED_MAT_ARITH(3)
    SF_DEFINE_MIXED_MAT_ARITH(4)

    SF_DEFINE_MIXED_MAT_VEC(2)
    SF_DEFINE_MIXED_MAT_VEC(3)
    SF_DEFINE_MIXED_MAT_VEC(4)

#undef SF_DEFINE_MIXED_MAT_VEC
#undef SF_DEFINE_MIXED_MAT_ARITH
#undef SF_DEFINE_MIXED_MAT_OP_SYM
#undef SF_DEFINE_MIXED_MAT_OP
#undef SF_DEFINE_SQUARE_MAT_TYPES
#undef SF_DEFINE_MAT_TYPES
} // namespace SF::Engine
