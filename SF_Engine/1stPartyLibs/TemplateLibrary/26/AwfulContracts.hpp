/***** REMOVE WHEN SWITCHING TO C++ 26 ****************************************/
/* AwfulContracts.hpp                                                         */
/******************************************************************************/
/*                            This file is part of                            */
/*             Scuffed Framework Standard Template Library                    */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Noah Lee                                        */
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

//
// A portable stand-in for C++26 Contracts (P2900R14) on compilers that don't
// have native support for the `pre`/`post`/`contract_assert` declarator syntax
// yet (as of writing: only GCC 16+ behind -std=c++26 -fcontracts; Clang and
// MSVC do not have it in mainline).
//
// IMPORTANT: `pre(...)` / `post(r: ...)` are core-language syntax that attach
// to a function's declarator, between the parameter list and the body. That
// is a grammar position, not an expression position -- a preprocessor macro
// cannot synthesize it, so there is no way to make your original source
//
//     double calculate_average(const std::vector<int>& scores)
//         pre(!scores.empty())
//         post(r: r >= 0.0)
//     { ... }
//
// compile unmodified on a pre-C++26 compiler. What this header gives you
// instead is a body-position fallback with the same checks, expressed with
// macros (SF_PRE / SF_POST / SF_CONTRACT_ASSERT), that:
//
//   - On a compiler that DOES have native contracts (__cpp_contracts defined),
//     these macros compile away to nothing -- write real pre/post/contract_assert
//     directly instead; this header then only supplies the violation-reporting
//     types below, mirroring std::contracts' shape.
//   - On a compiler that doesn't, they perform the equivalent runtime checks
//     from inside the function body.
//
// Rewritten example (see bottom of this comment):
//
//     double calculate_average(const std::vector<int>& scores)
//     {
//         SF_PRE(!scores.empty());
//
//         double sum = std::accumulate(scores.begin(), scores.end(), 0.0);
//         SF_CONTRACT_ASSERT(sum >= 0.0);
//
//         double r = sum / scores.size();
//         SF_POST(r, r >= 0.0);
//         return r;
//     }
//
// Note SF_POST must come after `r` is its final value but before `return`,
// since (unlike the real language feature, which checks at every return
// statement automatically) this is an RAII guard that checks in its
// destructor -- it needs `r` to be the actual object being returned, and it
// only fires once per guard, so with multiple return statements you'd want
// one guard per named return-value object, or to funnel returns through a
// single named local as shown above.
//
#pragma once

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <source_location>

namespace sf_contracts
{
    // Mirrors std::contracts::assertion_kind (P2900R14) in spirit; not the
    // literal std::contracts type since third-party code must not add
    // non-template entities into namespace std.
    enum class assertion_kind
    {
        pre = 1,
        post = 2,
        assert = 3,
    };

    struct contract_violation_info
    {
        assertion_kind kind;
        const char *comment; // stringized condition, e.g. "!scores.empty()"
        std::source_location location;
    };

    namespace detail
    {
        // Default reaction to a violation: report to stderr and terminate.
        // This deliberately does NOT throw -- contract violations are bugs,
        // not recoverable runtime errors, matching the "enforce" evaluation
        // semantic from P2900R14.
        inline void default_violation_handler(const contract_violation_info &v)
        {
            const char *kind_str = v.kind == assertion_kind::pre    ? "pre"
                                   : v.kind == assertion_kind::post ? "post"
                                                                    : "assert";
            std::fprintf(stderr,
                         "contract violation (%s): %s\n  at %s:%u in %s\n",
                         kind_str, v.comment,
                         v.location.file_name(), v.location.line(),
                         v.location.function_name());
            std::fflush(stderr);
            std::terminate();
        }

        // Portability note: the real std::contracts customization point is a
        // link-time override of a free function (like operator new), which
        // isn't expressible portably pre-C++26 without compiler-specific weak
        // symbols. This uses a runtime-settable handler instead -- assign to
        // it (e.g. in main(), or in a test fixture) to customize or to make
        // violations testable without actually terminating the process.
        inline std::function<void(const contract_violation_info &)> &violation_handler()
        {
            static std::function<void(const contract_violation_info &)> handler = default_violation_handler;
            return handler;
        }

        inline void check(bool cond, assertion_kind kind, const char *comment,
                          std::source_location loc = std::source_location::current())
        {
            if (!cond)
                violation_handler()({kind, comment, loc});
        }

        // RAII guard for postconditions: checks its predicate in its
        // destructor, but only on normal scope exit -- if the function is
        // instead leaving via an in-flight exception, the postcondition is
        // skipped, matching P2900R14 (a postcondition is only evaluated after
        // a function returns normally).
        class post_guard
        {
        public:
            post_guard(std::function<bool()> pred, const char *comment, std::source_location loc)
                : _pred(std::move(pred)), _comment(comment), _loc(loc), _uncaught_on_entry(std::uncaught_exceptions())
            {
            }

            post_guard(const post_guard &) = delete;
            post_guard &operator=(const post_guard &) = delete;

            // Destructors are implicitly noexcept(true) unless declared
            // otherwise -- since a caller-supplied violation_handler may
            // legitimately throw (e.g. to unit-test violations, or to unwind
            // instead of terminating), this must opt out of that default or
            // any such throw immediately calls std::terminate.
            ~post_guard() noexcept(false)
            {
                if (std::uncaught_exceptions() == _uncaught_on_entry && !_pred())
                    violation_handler()({assertion_kind::post, _comment, _loc});
            }

        private:
            std::function<bool()> _pred;
            const char *_comment;
            std::source_location _loc;
            int _uncaught_on_entry;
        };
    } // namespace detail
} // namespace sf_contracts

#define SF_CONTRACTS_CONCAT_(a, b) a##b
#define SF_CONTRACTS_CONCAT(a, b) SF_CONTRACTS_CONCAT_(a, b)

#if defined(__cpp_contracts)

// Native support is available -- use real pre/post/contract_assert directly
// in your source instead of these macros. Defined as no-ops so headers that
// unconditionally use SF_CONTRACT_ASSERT for body-only invariants still
// compile, but you should prefer the real `contract_assert(...)` here.
#define SF_PRE(cond) static_assert(true)
#define SF_POST(name, cond) static_assert(true)
#define SF_CONTRACT_ASSERT(cond) static_assert(true)

#else

#define SF_PRE(cond) \
    ::sf_contracts::detail::check((cond), ::sf_contracts::assertion_kind::pre, #cond)

#define SF_CONTRACT_ASSERT(cond) \
    ::sf_contracts::detail::check((cond), ::sf_contracts::assertion_kind::assert, #cond)

#define SF_POST(name, cond)                                                            \
    ::sf_contracts::detail::post_guard SF_CONTRACTS_CONCAT(_sf_post_guard_, __LINE__)( \
        [&]() -> bool { return (cond); }, #cond, std::source_location::current())

#endif