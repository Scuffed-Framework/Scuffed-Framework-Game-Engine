#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <type_traits>

namespace SF::Engine
{
// ---------------------------------------------------------------------------
// Type aliases: SF_DEFINE_VEC_TYPES(3) -> Vec3, DVec3, IVec3, UVec3, BVec3, ...
// ---------------------------------------------------------------------------
#define SF_DEFINE_VEC_TYPES(N)                                                                                         \
    using Vec##N     = glm::vec<N, float>;                                                                             \
    using DVec##N    = glm::vec<N, double>;                                                                            \
    using IVec##N    = glm::vec<N, int>;                                                                               \
    using UVec##N    = glm::vec<N, unsigned int>;                                                                      \
    using BVec##N    = glm::vec<N, bool>;                                                                              \
    using TVec##N    = glm::vec<N, std::uint16_t>;                                                                     \
    using I8Vec##N   = glm::vec<N, std::int8_t>;                                                                       \
    using I16Vec##N  = glm::vec<N, std::int16_t>;                                                                      \
    using I32Vec##N  = glm::vec<N, std::int32_t>;                                                                      \
    using I64Vec##N  = glm::vec<N, std::int64_t>;                                                                      \
    using Ui8Vec##N  = glm::vec<N, std::uint8_t>;                                                                      \
    using Ui16Vec##N = glm::vec<N, std::uint16_t>;                                                                     \
    using Ui32Vec##N = glm::vec<N, std::uint32_t>;                                                                     \
    using Ui64Vec##N = glm::vec<N, std::uint64_t>;                                                                     \
    inline TVec##N MakeTVec##N(std::uint16_t v) noexcept { return TVec##N(v); }

    SF_DEFINE_VEC_TYPES(2)
    SF_DEFINE_VEC_TYPES(3)
    SF_DEFINE_VEC_TYPES(4)

    inline TVec2 MakeTVec2(std::uint16_t x, std::uint16_t y) noexcept { return {x, y}; }

// ---------------------------------------------------------------------------
// Mixed-type operators. Works for any dimension because glm's converting
// constructors handle IVecN(UVecN) etc. Result type R is the "wider" side.
// ---------------------------------------------------------------------------
#define SF_DEFINE_MIXED_OP(OP, A, B, R)                                                                                \
    inline R operator OP(const A &lhs, const B &rhs) noexcept { return R(lhs) OP R(rhs); }

// Both orderings for one operator
#define SF_DEFINE_MIXED_OP_SYM(OP, A, B, R)                                                                            \
    SF_DEFINE_MIXED_OP(OP, A, B, R)                                                                                    \
    SF_DEFINE_MIXED_OP(OP, B, A, R)

// All four arithmetic operators for a pair of types
#define SF_DEFINE_MIXED_ARITH(A, B, R)                                                                                 \
    SF_DEFINE_MIXED_OP_SYM(+, A, B, R)                                                                                 \
    SF_DEFINE_MIXED_OP_SYM(-, A, B, R)                                                                                 \
    SF_DEFINE_MIXED_OP_SYM(*, A, B, R)                                                                                 \
    SF_DEFINE_MIXED_OP_SYM(/, A, B, R)

// Signed/unsigned mixing -> signed result, for each dimension
#define SF_DEFINE_INT_MIXING(N) SF_DEFINE_MIXED_ARITH(IVec##N, UVec##N, IVec##N)

    SF_DEFINE_INT_MIXING(2)
    SF_DEFINE_INT_MIXING(3)
    SF_DEFINE_INT_MIXING(4)

    // Float/int mixing -> float result, if you want it
#define SF_DEFINE_FLOAT_MIXING(N)                                                                                      \
    SF_DEFINE_MIXED_ARITH(Vec##N, IVec##N, Vec##N)                                                                     \
    SF_DEFINE_MIXED_ARITH(Vec##N, UVec##N, Vec##N)

    SF_DEFINE_FLOAT_MIXING(2)
    SF_DEFINE_FLOAT_MIXING(3)
    SF_DEFINE_FLOAT_MIXING(4)

#undef SF_DEFINE_FLOAT_MIXING
#undef SF_DEFINE_INT_MIXING
#undef SF_DEFINE_MIXED_ARITH
#undef SF_DEFINE_MIXED_OP_SYM
#undef SF_DEFINE_MIXED_OP
#undef SF_DEFINE_VEC_TYPES


    template<typename T, typename S>
    inline constexpr bool SFScalarOk =
            std::is_arithmetic_v<S> && !std::is_same_v<T, S> && (std::is_floating_point_v<T> || std::is_integral_v<S>);

#define SF_DEFINE_VEC_SCALAR_OP(OP)                                                                                    \
    template<glm::length_t L, typename T, glm::qualifier Q, typename S, std::enable_if_t<SFScalarOk<T, S>, int> = 0>   \
    inline glm::vec<L, T, Q> operator OP(const glm::vec<L, T, Q> &v, S s) noexcept                                     \
    {                                                                                                                  \
        return v OP static_cast<T>(s);                                                                                 \
    }                                                                                                                  \
    template<glm::length_t L, typename T, glm::qualifier Q, typename S, std::enable_if_t<SFScalarOk<T, S>, int> = 0>   \
    inline glm::vec<L, T, Q> operator OP(S s, const glm::vec<L, T, Q> &v) noexcept                                     \
    {                                                                                                                  \
        return static_cast<T>(s) OP v;                                                                                 \
    }

    SF_DEFINE_VEC_SCALAR_OP(+)
    SF_DEFINE_VEC_SCALAR_OP(-)
    SF_DEFINE_VEC_SCALAR_OP(*)
    SF_DEFINE_VEC_SCALAR_OP(/)

#undef SF_DEFINE_VEC_SCALAR_OP
} // namespace SF::Engine
