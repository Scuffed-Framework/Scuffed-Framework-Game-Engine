#pragma once
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <ID/GUID.hpp> // not a com ripoff
namespace SF::Engine::Reflection
{
    namespace detail
    {

        inline constexpr uint64_t kFNVBasis = 14695981039346656037ULL;
        inline constexpr uint64_t kFNVPrime = 1099511628211ULL;

        [[nodiscard]] constexpr uint64_t Fnv1a(std::string_view sv) noexcept
        {
            uint64_t h = kFNVBasis;
            for (unsigned char c : sv)
                h = (h ^ c) * kFNVPrime;
            return h;
        }

        //  Works by parsing the decorated signature of a dummy function.
        //  Result is a stable, human-readable name (no allocations, consteval-safe).
        template <typename T>
        [[nodiscard]] constexpr std::string_view RawTypeName() noexcept
        {
#if defined(__clang__)
            constexpr std::string_view fn = __PRETTY_FUNCTION__;
            constexpr std::string_view prefix = "[T = ";
            constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
            constexpr std::string_view fn = __PRETTY_FUNCTION__;
            constexpr std::string_view prefix = "[with T = ";
            constexpr std::string_view suffix = "]";
#elif defined(_MSC_VER)
            constexpr std::string_view fn = __FUNCSIG__;
            constexpr std::string_view prefix = "RawTypeName<";
            constexpr std::string_view suffix = ">(void)";
#else
#error "SF::Reflection: unsupported compiler; add your __PRETTY_FUNCTION__ prefix/suffix"
#endif

            const auto begin = fn.find(prefix);
            if (begin == std::string_view::npos)
                return fn;
            const auto start = begin + prefix.size();
            const auto end = fn.rfind(suffix);
            if (end == std::string_view::npos || end <= start)
                return fn;
            return fn.substr(start, end - start);
        }

        template <typename T, typename F>
        [[nodiscard]] constexpr std::size_t MemberOffset(F T::*member) noexcept
        {
            // Cast a null-derived address through the member pointer.
            // Defined behaviour under the common-initial-sequence rules of
            // major ABI implementations (Itanium, MSVC x64).
            return reinterpret_cast<std::size_t>(
                &(reinterpret_cast<const volatile T *>(0)->*member));
        }

    } // namespace detail

    // Stable, decoration-free type name at compile time (cv/ref stripped).
    template <typename T>
    [[nodiscard]] constexpr std::string_view TypeName() noexcept
    {
        return detail::RawTypeName<std::remove_cvref_t<T>>();
    }

    using TypeID = uint64_t;
    inline constexpr TypeID kNullTypeID = 0;

    template <typename T>
    [[nodiscard]] constexpr TypeID TypeIDOf() noexcept
    {
        return detail::Fnv1a(TypeName<T>());
    }

    struct PropertyInfo
    {
        std::string_view name;
        TypeID typeId = kNullTypeID;
        std::string_view typeName;
        std::size_t offset = 0; // byte offset inside the owning struct
        std::size_t size = 0;
        bool isConst : 1 {false};
        bool isPointer : 1 {false};

        std::function<void *(void *)> getter; // non-const instance
        std::function<const void *(const void *)> cGetter;
        std::function<void(void *, const void *)> setter;

        template <typename Field>
        Field &Get(void *instance) const
        {
            return *static_cast<Field *>(getter(instance));
        }
        template <typename Field>
        const Field &Get(const void *instance) const
        {
            return *static_cast<const Field *>(cGetter(instance));
        }
        template <typename Field>
        void Set(void *instance, const Field &value) const
        {
            setter(instance, &value);
        }
    };

    struct TypeInfo
    {
        // Identity
        std::string_view name;
        TypeID id = kNullTypeID;
        std::size_t size = 0;
        std::size_t alignment = 0;

        // Hierarchy
        TypeID baseId = kNullTypeID;      // single-inheritance base (0 = root)
        std::vector<TypeID> interfaceIds; // implemented interface IDs

        // Reflected properties (in registration order)
        std::vector<PropertyInfo> properties;

        std::function<void *(const void *)> copyConstruct; // heap-allocate + copy
        std::function<void *(void *)> moveConstruct;       // heap-allocate + move
        std::function<void(void *)> destruct;              // in-place destructor
        std::function<bool(const void *, const void *)> equal;

        // Optional stable serialization GUID
        GUID guid = GUID::Generate();

        // ── Queries ──────────────────────────────────────────────────────────────
        [[nodiscard]] const PropertyInfo *FindProperty(std::string_view n) const noexcept
        {
            for (const auto &p : properties)
                if (p.name == n)
                    return &p;
            return nullptr;
        }

        /// True if this type IS queryId or directly inherits from it.
        [[nodiscard]] bool IsA(TypeID queryId) const noexcept
        {
            return (id == queryId) || (baseId == queryId);
        }

        [[nodiscard]] bool ImplementsInterface(TypeID iid) const noexcept
        {
            return std::find(interfaceIds.begin(), interfaceIds.end(), iid) != interfaceIds.end();
        }
    };

    //  § 5  TypeRegistry  –  global singleton, O(1) by ID

    class TypeRegistry final
    {
    public:
        TypeRegistry(const TypeRegistry &) = delete;
        TypeRegistry &operator=(const TypeRegistry &) = delete;

        [[nodiscard]] static TypeRegistry &Instance() noexcept
        {
            static TypeRegistry s;
            return s;
        }

        void Register(TypeInfo info)
        {
            assert(info.id != kNullTypeID && "TypeID must not be zero – hash collision?");
            m_byId[info.id] = std::move(info);
        }

        // ── Lookup ───────────────────────────────────────────────────────────────
        [[nodiscard]] const TypeInfo *Find(TypeID id) const noexcept
        {
            auto it = m_byId.find(id);
            return (it != m_byId.end()) ? &it->second : nullptr;
        }

        [[nodiscard]] const TypeInfo *Find(std::string_view name) const noexcept
        {
            for (const auto &[id, info] : m_byId)
                if (info.name == name)
                    return &info;
            return nullptr;
        }

        template <typename T>
        [[nodiscard]] const TypeInfo *Find() const noexcept
        {
            return Find(TypeIDOf<T>());
        }

        /// Walk the inheritance chain upward until queryId is found (or root).
        [[nodiscard]] bool IsA(TypeID derived, TypeID base) const noexcept
        {
            for (TypeID cur = derived; cur != kNullTypeID;)
            {
                if (cur == base)
                    return true;
                const TypeInfo *info = Find(cur);
                if (!info)
                    break;
                cur = info->baseId;
            }
            return false;
        }

        template <typename Derived, typename Base>
        [[nodiscard]] bool IsA() const noexcept
        {
            return IsA(TypeIDOf<Derived>(), TypeIDOf<Base>());
        }

        [[nodiscard]] std::span<const TypeInfo *const> All() const noexcept
        {
            // Rebuild the flat view only when the registry changes.
            if (m_flatDirty)
            {
                m_flat.clear();
                m_flat.reserve(m_byId.size());
                for (const auto &[id, info] : m_byId)
                    m_flat.push_back(&info);
                m_flatDirty = false;
            }
            return m_flat;
        }

    private:
        TypeRegistry() = default;

        std::unordered_map<TypeID, TypeInfo> m_byId;

        mutable std::vector<const TypeInfo *> m_flat;
        mutable bool m_flatDirty = true;
    };

    template <typename T>
    class TypeInfoBuilder final
    {
    public:
        TypeInfoBuilder() noexcept
        {
            m_info.name = TypeName<T>();
            m_info.id = TypeIDOf<T>();
            m_info.size = sizeof(T);
            m_info.alignment = alignof(T);

            // Auto-generate lifecycle hooks where possible.
            if constexpr (std::is_copy_constructible_v<T>)
                m_info.copyConstruct = [](const void *src) -> void *
                {
                    return new T(*static_cast<const T *>(src));
                };

            if constexpr (std::is_move_constructible_v<T>)
                m_info.moveConstruct = [](void *src) -> void *
                {
                    return new T(std::move(*static_cast<T *>(src)));
                };

            if constexpr (std::is_destructible_v<T>)
                m_info.destruct = [](void *ptr)
                {
                    static_cast<T *>(ptr)->~T();
                };

            if constexpr (std::equality_comparable<T>)
                m_info.equal = [](const void *a, const void *b) -> bool
                {
                    return *static_cast<const T *>(a) == *static_cast<const T *>(b);
                };
        }

        template <typename Base>
        TypeInfoBuilder &Extends() noexcept
        {
            static_assert(std::is_base_of_v<Base, T>,
                          "Extends<Base>: Base is not an actual base class of T");
            m_info.baseId = TypeIDOf<Base>();
            return *this;
        }

        template <typename Interface>
        TypeInfoBuilder &Implements() noexcept
        {
            m_info.interfaceIds.push_back(TypeIDOf<Interface>());
            return *this;
        }

        TypeInfoBuilder &WithGUID(GUID guid) noexcept
        {
            m_info.guid = guid;
            return *this;
        }

        TypeInfoBuilder &WithName(std::string_view override) noexcept
        {
            m_info.name = override; // e.g. strip namespace noise
            return *this;
        }

        template <typename Field>
        TypeInfoBuilder &Property(std::string_view propName, Field T::*member)
        {
            PropertyInfo p;
            p.name = propName;
            p.typeId = TypeIDOf<Field>();
            p.typeName = TypeName<Field>();
            p.offset = detail::MemberOffset(member);
            p.size = sizeof(Field);
            p.isConst = std::is_const_v<Field>;
            p.isPointer = std::is_pointer_v<Field>;

            p.getter = [member](void *inst) -> void *
            {
                return static_cast<void *>(&(static_cast<T *>(inst)->*member));
            };
            p.cGetter = [member](const void *inst) -> const void *
            {
                return static_cast<const void *>(&(static_cast<const T *>(inst)->*member));
            };
            p.setter = [member](void *inst, const void *val)
            {
                if constexpr (!std::is_const_v<Field>)
                    static_cast<T *>(inst)->*member = *static_cast<const Field *>(val);
            };

            m_info.properties.push_back(std::move(p));
            return *this;
        }

        /// Push the descriptor into the global registry.
        void Register()
        {
            TypeRegistry::Instance().Register(std::move(m_info));
        }

    private:
        TypeInfo m_info;
    };

    // Satisfied when T has been registered with the TypeRegistry.
    template <typename T>
    concept Reflected = requires {
        { TypeRegistry::Instance().Find<T>() } -> std::convertible_to<const TypeInfo *>;
    } && (TypeRegistry::Instance().Find<T>() != nullptr);

//  Usage example
//  struct Foo : Bar { int x; float y; };
//
//  SF_REFLECT(Foo)
//      SF_EXTENDS(Bar)
//      SF_PROP(x)
//      SF_PROP(y)
//  SF_REFLECT_END()
//
//  Tip: place inside a translation unit (.cpp) or an inline init function
//  called from main() / a module initialiser.

// Opens a builder expression for Type.
#define SF_REFLECT(Type) \
    (void)(::SF::Engine::Reflection::TypeInfoBuilder<Type>{}

// Extends from a base class.
#define SF_EXTENDS(Base) \
    .template Extends<Base>()

// Implements an interface.
#define SF_IMPLEMENTS(Iface) \
    .template Implements<Iface>()

// Reflects a member field.  Member name becomes the property name.
#define SF_PROP(Member) \
    .Property(#Member, &std::remove_pointer_t<decltype(this)>::Member)

// Reflects with a custom string name.
#define SF_PROP_AS(Member, Name) \
    .Property(Name, &std::remove_pointer_t<decltype(this)>::Member)

// Sets the GUID for serialization.
#define SF_GUID(GuidLiteral) \
    .WithGUID(GuidLiteral)

// Closes the builder expression and registers the type.
#define SF_REFLECT_END() \
    .Register())

} // namespace SF::Engine::Reflection
