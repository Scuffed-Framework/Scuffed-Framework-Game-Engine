#pragma once
#ifndef IM_ASSERT
    #include <cassert>
    #define IM_ASSERT(_EXPR) assert(_EXPR) // You can override the default assert handler by editing imconfig.h
#endif
#define IM_COUNTOF(_ARR)                                                                                               \
    ((int) (sizeof(_ARR) / sizeof(*(_ARR)))) // Size of a static C-style array. Don't use on pointers!
#define IM_UNUSED(_VAR)                                                                                                \
    ((void) (_VAR)) // Used to silence "unused variable warnings". Often useful as asserts may be stripped out from
                    // final builds.
#define IM_STRINGIFY_HELPER(_EXPR) #_EXPR
#define IM_STRINGIFY(_EXPR) IM_STRINGIFY_HELPER(_EXPR) // Preprocessor idiom to stringify e.g. an integer or a macro.


// Helper Macros - IM_FMTARGS, IM_FMTLIST: Apply printf-style warnings to our formatting functions.
// (MSVC provides an equivalent mechanism via SAL Annotations but it requires the macros in a different
//  location. e.g. #include <sal.h> + void myprintf(_Printf_format_string_ const char* format, ...),
//  and only works when using Code Analysis, rather than just normal compiling).
// (see https://github.com/ocornut/imgui/issues/8871 for a patch to enable this for MSVC's Code Analysis)
#if !defined(IMGUI_USE_STB_SPRINTF) && defined(__MINGW32__) && !defined(__clang__)
    #define IM_FMTARGS(FMT) __attribute__((format(gnu_printf, FMT, FMT + 1)))
    #define IM_FMTLIST(FMT) __attribute__((format(gnu_printf, FMT, 0)))
#elif !defined(IMGUI_USE_STB_SPRINTF) && (defined(__clang__) || defined(__GNUC__))
    #define IM_FMTARGS(FMT) __attribute__((format(printf, FMT, FMT + 1)))
    #define IM_FMTLIST(FMT) __attribute__((format(printf, FMT, 0)))
#else
    #define IM_FMTARGS(FMT)
    #define IM_FMTLIST(FMT)
#endif

// Disable some of MSVC most aggressive Debug runtime checks in function header/footer (used in some
// simple/low-level functions)
#if defined(_MSC_VER) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(IMGUI_DEBUG_PARANOID)
    #define IM_MSVC_RUNTIME_CHECKS_OFF                                                                                 \
        __pragma(runtime_checks("", off)) __pragma(check_stack(off)) __pragma(strict_gs_check(push, off))
    #define IM_MSVC_RUNTIME_CHECKS_RESTORE                                                                             \
        __pragma(runtime_checks("", restore)) __pragma(check_stack()) __pragma(strict_gs_check(pop))
#else
    #define IM_MSVC_RUNTIME_CHECKS_OFF
    #define IM_MSVC_RUNTIME_CHECKS_RESTORE
#endif

// Warnings
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 26495) // [Static Analyzer] Variable 'XXX' is uninitialized. Always initialize a
                                     // member variable (type.6).
#endif
#if defined(__clang__)
    #pragma clang diagnostic push
    #if __has_warning("-Wunknown-warning-option")
        #pragma clang diagnostic ignored "-Wunknown-warning-option" // warning: unknown warning group 'xxx'
    #endif
    #pragma clang diagnostic ignored "-Wunknown-pragmas" // warning: unknown warning group 'xxx'
    #pragma clang diagnostic ignored "-Wold-style-cast"  // warning: use of old-style cast
    #pragma clang diagnostic ignored "-Wfloat-equal"     // warning: comparing floating point with == or != is unsafe
    #pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant" // warning: zero as null pointer constant
    #pragma clang diagnostic ignored "-Wreserved-identifier" // warning: identifier '_Xxx' is reserved because it
                                                             // starts with '_' followed by a capital letter
    #pragma clang diagnostic ignored                                                                                   \
            "-Wunsafe-buffer-usage" // warning: 'xxx' is an unsafe pointer used for buffer access
    #pragma clang diagnostic ignored "-Wnontrivial-memaccess" // warning: first argument in call to 'memset' is a
                                                              // pointer to non-trivially copyable type
#endif

#if defined(_WIN32) && !defined(_MSC_VER) && !defined(IMGUI_ENABLE_WIN32_DEFAULT_IME_FUNCTIONS) &&                     \
        !defined(IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS)
    #define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
#endif

// [Windows] OS specific includes (optional)
#if defined(_WIN32) && defined(IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS) &&                                                \
        defined(IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS) &&                                                    \
        defined(IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS) && defined(IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS) &&        \
        !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
    #define IMGUI_DISABLE_WIN32_FUNCTIONS
#endif
#if defined(_WIN32) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef __MINGW32__
        #include <Windows.h> // _wfopen, OpenClipboard
    #else
        #include <windows.h>
    #endif
    #if defined(WINAPI_FAMILY) && ((defined(WINAPI_FAMILY_APP) && WINAPI_FAMILY == WINAPI_FAMILY_APP) ||               \
                                   (defined(WINAPI_FAMILY_GAMES) && WINAPI_FAMILY == WINAPI_FAMILY_GAMES))
        // The UWP and GDK Win32 API subsets don't support clipboard nor IME functions
        #define IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
        #define IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
        #define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
    #endif
#endif

// [Apple] OS specific includes
#if defined(__APPLE__)
    #include <TargetConditionals.h>
#endif

// Visual Studio warnings
#ifdef _MSC_VER
    #pragma warning(disable : 4127)           // condition expression is constant
    #pragma warning(disable : 4996)           // 'This function or variable may be unsafe': strcpy, strdup, sprintf,
                                              // vsnprintf, sscanf, fopen
    #if defined(_MSC_VER) && _MSC_VER >= 1922 // MSVC 2019 16.2 or later
        #pragma warning(disable : 5054)       // operator '|': deprecated between enumerations of different types
    #endif
    #pragma warning(disable : 26451) // [Static Analyzer] Arithmetic overflow : Using operator 'xxx' on a 4 byte
                                     // value and then casting the result to an 8 byte value. Cast the value to the
                                     // wider type before calling operator 'xxx' to avoid overflow(io.2).
    #pragma warning(disable : 26495) // [Static Analyzer] Variable 'XXX' is uninitialized. Always initialize a
                                     // member variable (type.6).
    #pragma warning(disable : 26812) // [Static Analyzer] The enum type 'xxx' is unscoped. Prefer 'enum class' over
                                     // 'enum' (Enum.3).
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
    #if __has_warning("-Wunknown-warning-option")
        #pragma clang diagnostic ignored                                                                               \
                "-Wunknown-warning-option" // warning: unknown warning group 'xxx'                      // not all
                                           // warnings are known by all Clang versions and they tend to be
                                           // rename-happy.. so ignoring warnings triggers new warnings on some
                                           // configuration. Great!
    #endif
    #pragma clang diagnostic ignored "-Wunknown-pragmas" // warning: unknown warning group 'xxx'
    #pragma clang diagnostic ignored "-Wold-style-cast"  // warning: use of old-style cast // yes, they are more
                                                         // terse.
    #pragma clang diagnostic ignored                                                                                   \
            "-Wfloat-equal" // warning: comparing floating point with == or != is unsafe // storing and comparing
                            // against same constants (typically 0.0f) is ok.
    #pragma clang diagnostic ignored                                                                                   \
            "-Wformat" // warning: format specifies type 'int' but the argument has type 'unsigned int'
    #pragma clang diagnostic ignored "-Wformat-nonliteral" // warning: format string is not a string literal //
                                                           // passing non-literal to vsnformat(). yes, user passing
                                                           // incorrect format strings can crash the code.
    #pragma clang diagnostic ignored "-Wformat-pedantic"   // warning: format specifies type 'void *' but the argument
                                                           // has type 'xxxx *' // unreasonable, would lead to casting
                                                           // every %p arg to void*. probably enabled by -pedantic.
    #pragma clang diagnostic ignored                                                                                   \
            "-Wexit-time-destructors" // warning: declaration requires an exit-time destructor     // exit-time
                                      // destruction order is undefined. if MemFree() leads to users code that has
                                      // been disabled before exit it might cause problems. ImGui coding style
                                      // welcomes static/globals.
    #pragma clang diagnostic ignored                                                                                   \
            "-Wglobal-constructors" // warning: declaration requires a global destructor         // similar to
                                    // above, not sure what the exact difference is.
    #pragma clang diagnostic ignored "-Wsign-conversion" // warning: implicit conversion changes signedness
    #pragma clang diagnostic ignored                                                                                   \
            "-Wint-to-void-pointer-cast" // warning: cast to 'void *' from smaller integer type 'int'
    #pragma clang diagnostic ignored                                                                                   \
            "-Wzero-as-null-pointer-constant" // warning: zero as null pointer constant                    // some
                                              // standard header variations use #define nullptr 0
    #pragma clang diagnostic ignored "-Wdouble-promotion" // warning: implicit conversion from 'float' to 'double' when
                                                          // passing argument to function  // using printf() is a misery
                                                          // with this as C++ va_arg ellipsis changes float to double.
    #pragma clang diagnostic ignored "-Wimplicit-int-float-conversion" // warning: implicit conversion from 'xxx' to
                                                                       // 'float' may lose precision
    #pragma clang diagnostic ignored                                                                                   \
            "-Wunsafe-buffer-usage" // warning: 'xxx' is an unsafe pointer used for buffer access
    #pragma clang diagnostic ignored "-Wnontrivial-memaccess" // warning: first argument in call to 'memset' is a
                                                              // pointer to non-trivially copyable type
    #pragma clang diagnostic ignored "-Wswitch-default"       // warning: 'switch' missing 'default' label
#elif defined(__GNUC__)
    // We disable -Wpragmas because GCC doesn't provide a has_warning equivalent and some forks/patches may not
    // follow the warning/version association.
    #pragma GCC diagnostic ignored "-Wpragmas"         // warning: unknown option after '#pragma GCC diagnostic' kind
    #pragma GCC diagnostic ignored "-Wunused-function" // warning: 'xxxx' defined but not used
    #pragma GCC diagnostic ignored "-Wint-to-pointer-cast" // warning: cast to pointer from integer of different size
    #pragma GCC diagnostic ignored "-Wfloat-equal" // warning: comparing floating-point with '==' or '!=' is unsafe
    #pragma GCC diagnostic ignored "-Wformat"      // warning: format '%p' expects argument of type 'int'/'void*', but
                                                   // argument X has type 'unsigned int'/'ImGuiWindow*'
    #pragma GCC diagnostic ignored "-Wdouble-promotion" // warning: implicit conversion from 'float' to 'double'
                                                        // when passing argument to function
    #pragma GCC diagnostic ignored "-Wconversion"       // warning: conversion to 'xxxx' from 'xxxx' may alter its value
    #pragma GCC diagnostic ignored                                                                                     \
            "-Wformat-nonliteral" // warning: format not a string literal, format string not checked
    #pragma GCC diagnostic ignored "-Wstrict-overflow" // warning: assuming signed overflow does not occur when
                                                       // assuming that (X - c) > X is always false
    #pragma GCC diagnostic ignored "-Wclass-memaccess" // [__GNUC__ >= 8] warning: 'memset/memcpy' clearing/writing
                                                       // an object of type 'xxxx' with no trivial copy-assignment;
                                                       // use assignment or value-initialization instead
    #pragma GCC diagnostic ignored                                                                                     \
            "-Wcast-qual" // warning: cast from type 'const xxxx *' to type 'xxxx *' casts away qualifiers
    #pragma GCC diagnostic ignored                                                                                     \
            "-Wsign-conversion" // warning: conversion to 'xxxx' from 'xxxx' may change the sign of the result
#endif
