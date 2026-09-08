#pragma once

#include <cstdlib>
#include "Types.hpp"


namespace SFTL::Detail
{
    [[nodiscard]] inline void *AlignedMalloc(size_type size, align_value_type align) noexcept
    {
        const auto a = static_cast<size_type>(align);

        // aligned_alloc requires size to be a multiple of alignment
        const size_type rounded = (size + a - 1) & ~(a - 1);

        return std::aligned_alloc(a, rounded);
    }

    inline void AlignedFree(void *p) noexcept { std::free(p); }
} // namespace SFTL::Detail

[[nodiscard]] inline void *operator new(unsigned long size, ::SFTL::align_value_type align,
                                        const ::SFTL::nothrow_type &) noexcept
{
    return ::SFTL::Detail::AlignedMalloc(size, align);
}

inline void operator delete(void *p, ::SFTL::align_value_type) noexcept { ::SFTL::Detail::AlignedFree(p); }
