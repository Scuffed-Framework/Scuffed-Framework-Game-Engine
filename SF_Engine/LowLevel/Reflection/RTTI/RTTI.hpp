#pragma once
//
// SF Engine :: Reflection :: RTTI.h
//
// Usage:
//
//   class IPipelinePass
//   {
//       SF_RTTI_BASE(IPipelinePass)
//   public:
//       virtual ~IPipelinePass() = default;
//       virtual void Execute() = 0;
//   };
//
//   class ComputePipelinePass : public IPipelinePass
//   {
//       SF_RTTI(ComputePipelinePass, IPipelinePass)
//   public:
//       void Execute() override { /* ... */ }
//   };
//
//   class AtmosphereLUTPass : public ComputePipelinePass
//   {
//       SF_RTTI(AtmosphereLUTPass, ComputePipelinePass)
//   public:
//       void Bake() { /* ... */ }
//   };
//
// Rules:
//   - Exactly one SF_RTTI_BASE per hierarchy, at the root (the class that
//     first declares the virtual interface). The root must have a virtual
//     destructor (add it yourself alongside the macro, as above).
//   - Every derived class uses SF_RTTI(ClassName, Base1[, Base2, ...]) and
//     lists ALL of its direct bases that themselves use SF_RTTI/SF_RTTI_BASE.
//     Multiple inheritance is fully supported.
//   - Put the macro as the first line in the class body; it opens with
//     `public:` and closes with `private:`, so anything you write below it
//     defaults back to private as normal.
//
#include "TypeId.hpp"

namespace SF::RTTI::Detail
{
    // Fold-expression OR across an arbitrary number of direct bases.
    // Empty pack case intentionally not supported here -- SF_RTTI always
    // requires >=1 base; use SF_RTTI_BASE for the hierarchy root instead.
    template <typename... Bases>
    constexpr bool AnyBaseIsTypeOf(::SF::RTTI::TypeId id)
    {
        static_assert(sizeof...(Bases) > 0,
                      "SF_RTTI requires at least one base class. Use SF_RTTI_BASE for the hierarchy root.");
        return (Bases::RTTI_IsTypeOfStatic(id) || ...);
    }
}

// Lightweight, non-virtual: just gives a plain struct a TypeId + name so it
// can be registered with SerializeContext. Use this for data-only types that
// aren't part of a polymorphic hierarchy (shader constant blocks, POD structs).
// Zero runtime cost -- no vtable added. Ends in `public:` (not `private:`)
// since these types are typically plain structs with public members.
#define SF_TYPE_INFO(ClassName)                                         \
public:                                                                 \
    using RTTI_ThisType = ClassName;                                    \
    static constexpr ::SF::RTTI::TypeId RTTI_Type()                     \
    {                                                                   \
        return ::SF::RTTI::GetTypeId<ClassName>();                      \
    }                                                                   \
    static constexpr const char *RTTI_TypeName() { return #ClassName; } \
                                                                        \
public:

// Declares the root of an RTTI hierarchy.
#define SF_RTTI_BASE(ClassName)                                                                 \
public:                                                                                         \
    using RTTI_ThisType = ClassName;                                                            \
    static constexpr ::SF::RTTI::TypeId RTTI_Type()                                             \
    {                                                                                           \
        return ::SF::RTTI::GetTypeId<ClassName>();                                              \
    }                                                                                           \
    static constexpr const char *RTTI_TypeName() { return #ClassName; }                         \
    static bool RTTI_IsTypeOfStatic(::SF::RTTI::TypeId id)                                      \
    {                                                                                           \
        return id == RTTI_Type();                                                               \
    }                                                                                           \
    virtual ::SF::RTTI::TypeId RTTI_GetType() const { return RTTI_Type(); }                     \
    virtual const char *RTTI_GetTypeName() const { return RTTI_TypeName(); }                    \
    virtual bool RTTI_IsTypeOf(::SF::RTTI::TypeId id) const { return RTTI_IsTypeOfStatic(id); } \
                                                                                                \
private:

// Declares a derived node in an RTTI hierarchy. List every direct base
// that itself carries SF_RTTI / SF_RTTI_BASE.
#define SF_RTTI(ClassName, ...)                                                                  \
public:                                                                                          \
    using RTTI_ThisType = ClassName;                                                             \
    static constexpr ::SF::RTTI::TypeId RTTI_Type()                                              \
    {                                                                                            \
        return ::SF::RTTI::GetTypeId<ClassName>();                                               \
    }                                                                                            \
    static constexpr const char *RTTI_TypeName() { return #ClassName; }                          \
    static bool RTTI_IsTypeOfStatic(::SF::RTTI::TypeId id)                                       \
    {                                                                                            \
        return id == RTTI_Type() || ::SF::RTTI::Detail::AnyBaseIsTypeOf<__VA_ARGS__>(id);        \
    }                                                                                            \
    ::SF::RTTI::TypeId RTTI_GetType() const override { return RTTI_Type(); }                     \
    const char *RTTI_GetTypeName() const override { return RTTI_TypeName(); }                    \
    bool RTTI_IsTypeOf(::SF::RTTI::TypeId id) const override { return RTTI_IsTypeOfStatic(id); } \
                                                                                                 \
private: