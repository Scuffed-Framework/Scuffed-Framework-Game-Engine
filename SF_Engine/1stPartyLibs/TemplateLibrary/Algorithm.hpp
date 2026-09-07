#pragma once

namespace SFTL
{
    template<typename Type>
    [[nodiscard]] constexpr const Type &min(const Type &A, const Type &B)
    {
        if (B < A)
            return B;
        return A;
    }

    template<typename Type>
    [[nodiscard]] constexpr const Type &max(const Type &A, const Type &B)
    {
        if (B > A)
            return B;
        return A;
    }

    template<typename InputIterator1, typename InputIterator2>
    [[nodiscard]] constexpr inline bool equal(InputIterator1 FirstA, InputIterator1 Last, InputIterator2 FirstB)
    {
        for (; FirstA != Last; ++FirstA, ++FirstB)
        {
            if (!(*FirstA == *FirstB))
                return false;
        }
        return true;
    }

    template<typename InputIterator1, typename InputIterator2, typename BinaryPredicate>
    [[nodiscard]] constexpr inline bool equal(InputIterator1 FirstA, InputIterator1 Last, InputIterator2 FirstB,
                                              BinaryPredicate p)
    {
        for (; FirstA != Last; ++FirstA, ++FirstB)
        {
            if (!p(*FirstA, *FirstB))
                return false;
        }
        return true;
    }
} // namespace SFTL
