#pragma once
#define NO_DISCARD [[nodiscard]]
#if defined(_MSC_VER) && !defined(__clang__)
    #define FORCE_INLINE __forceinline
#else
    #define FORCE_INLINE inline __attribute__((__always_inline__))
#endif
#define UNUSED __attribute__((__unused__))
