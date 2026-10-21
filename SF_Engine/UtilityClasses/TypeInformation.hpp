/******************************************************************************/
/* TypeInformation.hpp                                                        */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
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

#pragma once

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <spdlog/spdlog.h>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <LowLevel/Reflection/RTTI/TypeId.hpp>

// This header used to mint its own type ids: a std::type_index -> a
// sequentially-assigned std::size_t, in a map kept per-hierarchy (one map
// per distinct T), guarded by a shared_mutex. That was a second, independent
// identity scheme living alongside SF::RTTI::TypeId (this file even included
// RTTI's TypeId.hpp already, without using it). It has been merged away:
// there is now exactly one type id in the engine, minted by
// SF::RTTI::GetTypeId<T>() -- constexpr, no locking, and identical for a
// given type whether it's reached through SF_RTTI/SF_RTTI_BASE or only ever
// seen generically through a template parameter, as below.
//
// TypeInformation<T> keeps existing as a thin, per-hierarchy convenience
// layer over that single id, plus an *optional* runtime registry for
// introspection -- e.g. "which concrete subtypes of T have actually been
// registered" for an editor/tooling type browser. That registry is now
// purely additive bookkeeping: it no longer determines or gates any type's
// id, so GetTypeId<K>() works for any K regardless of whether Register<K>()
// was ever called.
//
// Breaking change from the old version: GetTypeId<K>() now returns
// SF::RTTI::TypeId (a struct wrapping uint64_t), not a bare std::size_t.
// Code that did arithmetic or raw-integer comparisons on the old return
// value needs to go through TypeId::value instead.
namespace SF::Engine
{
    using TypeId = ::SF::RTTI::TypeId;

    /**
     * @brief Thread-safe, per-hierarchy convenience wrapper around
     *        SF::RTTI's unified TypeId, plus an opt-in introspection
     *        registry.
     * @tparam T Base type for the type hierarchy
     */
    template<typename T>
    class TypeInformation
    {
    public:
        TypeInformation() = delete;

        /**
         * @brief Get the type ID for a derived type K.
         * @tparam K The derived type. Deliberately unconstrained (no
         *         is_convertible_v<K*, T*> check): this is called from
         *         virtual overrides in CRTP registrar patterns (e.g.
         *         StreamFactory<T>::Registrar<K>), and implicit
         *         instantiation of such a class template instantiates its
         *         virtual member bodies immediately -- while K (the
         *         still-being-defined derived class) is incomplete.
         *         is_convertible_v requires complete types, so a hierarchy
         *         check here would hard-fail on exactly the pattern this
         *         is meant to support. If you want that safety net, assert
         *         it explicitly at a call site where K is already complete
         *         (e.g. right after K's definition, or in a REGISTER_STREAM-
         *         style macro) with static_assert(std::is_convertible_v<K*, T*>).
         * @return The same id SF::RTTI::GetTypeId<K>() returns -- constexpr,
         *         and independent of whether K was ever Register()ed.
         */
        template<typename K>
        [[nodiscard]] static constexpr TypeId GetTypeId() noexcept
        {
            return ::SF::RTTI::GetTypeId<K>();
        }

        /**
         * @brief Get a human-readable name for a derived type K.
         * @tparam K The derived type. Unconstrained for the same reason as
         *         GetTypeId() above -- reachable through the same virtual,
         *         forced-instantiation CRTP path while K is incomplete.
         * @return A demangled name, cached for the lifetime of the program
         *         (K is not required to carry a compile-time name -- types
         *         declared with SF_RTTI/SF_RTTI_BASE should prefer their own
         *         RTTI_TypeName()/RTTI_GetTypeName() instead).
         */
        template<typename K>
        [[nodiscard]] static std::string_view GetTypeName() noexcept
        {
            static const std::string name = ::SF::RTTI::GetTypeName<K>();
            return name;
        }

        /**
         * @brief Record K in this hierarchy's introspection registry.
         *        Purely additive bookkeeping -- does not affect K's id,
         *        which is always available via GetTypeId<K>() regardless.
         *        Unlike GetTypeId()/GetTypeName() above, this keeps the
         *        is_convertible_v<K*, T*> check: Register() is an ordinary
         *        (non-virtual) function template, so it's only instantiated
         *        when something actually calls it -- normally well after K
         *        is complete (e.g. from a REGISTER_STREAM-style macro at
         *        namespace scope) -- so the completeness requirement is safe
         *        here and the check is worth keeping.
         * @return K's (unaffected) type id, for convenience at call sites
         *         that used to rely on registration to produce the id.
         */
        template<typename K>
            requires std::is_convertible_v<K *, T *>
        static TypeId Register() noexcept
        {
            const TypeId id = ::SF::RTTI::GetTypeId<K>();
            std::unique_lock lock(s_mutex);
            s_registry.try_emplace(id, ::SF::RTTI::GetTypeName<K>());
            return id;
        }

        /**
         * @brief Number of types explicitly recorded via Register<K>() for
         *        this hierarchy. This is NOT the number of types with a
         *        valid id -- every type has one; this only counts the ones
         *        that opted into the introspection registry.
         */
        [[nodiscard]] static size_t GetRegisteredTypeCount() noexcept
        {
            std::shared_lock lock(s_mutex);
            return s_registry.size();
        }

        /**
         * @brief Whether K has been recorded via Register<K>() for this
         *        hierarchy. A false result does not mean K lacks an id.
         */
        template<typename K>
            requires std::is_convertible_v<K *, T *>
        [[nodiscard]] static bool IsRegistered() noexcept
        {
            std::shared_lock lock(s_mutex);
            return s_registry.contains(::SF::RTTI::GetTypeId<K>());
        }

        /**
         * @brief Clear this hierarchy's introspection registry. Does not,
         *        and cannot, affect any type's id.
         */
        static void Clear() noexcept
        {
            std::unique_lock lock(s_mutex);
            s_registry.clear();
        }

    private:
        inline static std::unordered_map<TypeId, std::string> s_registry = {};
        inline static std::shared_mutex s_mutex;
    };

    /**
     * @brief Alias for convenience
     */
    template<typename T>
    using TypeInfo = TypeInformation<T>;
} // namespace SF::Engine

#define Define_TypeId_Function(base, cl)                                                                               \
    [[nodiscard]] ::SF::RTTI::TypeId GetTypeId() const override { return ::SF::RTTI::GetTypeId<cl>(); }

#define TypeId_Name_Function(cl)                                                                                       \
    [[nodiscard]] ::std::string_view GetTypeName() const override                                                      \
    {                                                                                                                  \
        static const ::std::string name = ::SF::RTTI::GetTypeName<cl>();                                               \
        return name;                                                                                                   \
    }


template<>
struct fmt::formatter<SF::RTTI::TypeId>
{
    constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

    template<typename FormatContext>
    auto format(const SF::RTTI::TypeId &typeId, FormatContext &ctx) const
    {
        return fmt::format_to(ctx.out(), "{}", typeId.value);
    }
};
