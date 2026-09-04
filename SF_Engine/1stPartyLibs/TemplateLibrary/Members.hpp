#pragma once

namespace SFTL
{
    struct EnableDefaultConstructorTag
    {
        explicit constexpr EnableDefaultConstructorTag() = default;
    };

    template<bool Switch, typename Tag = void>
    struct EnableDefaultConstructor
    {
        constexpr EnableDefaultConstructor() noexcept                                 = default;
        constexpr EnableDefaultConstructor(EnableDefaultConstructor const &) noexcept = default;
        constexpr EnableDefaultConstructor(EnableDefaultConstructor &&) noexcept      = default;

        EnableDefaultConstructor &operator=(EnableDefaultConstructor const &) noexcept = default;

        EnableDefaultConstructor &operator=(EnableDefaultConstructor &&) noexcept = default;

        constexpr explicit EnableDefaultConstructor(EnableDefaultConstructorTag) {}
    };

    template<bool Switch, typename Tag = void>
    struct EnableDestructor
    {
    };

    template<bool Copy, bool CopyAssignment, bool Move, bool MoveAssignment, typename Tag = void>
    struct EnableCopyMove
    {
    };

    template<bool Default, bool Destructor, bool Copy, bool CopyAssignment, bool Move, bool MoveAssignment,
             typename Tag = void>
    struct EnableSpecialMembers : private EnableDefaultConstructor<Default, Tag>,
                                  private EnableDestructor<Destructor, Tag>,
                                  private EnableCopyMove<Copy, CopyAssignment, Move, MoveAssignment, Tag>
    {
    };

    template<typename Tag>
    struct EnableDefaultConstructor<false, Tag>
    {
        constexpr EnableDefaultConstructor() noexcept = delete;

        constexpr EnableDefaultConstructor(EnableDefaultConstructor const &) noexcept = default;

        constexpr EnableDefaultConstructor(EnableDefaultConstructor &&) noexcept = default;

        EnableDefaultConstructor &operator=(EnableDefaultConstructor const &) noexcept = default;

        EnableDefaultConstructor &operator=(EnableDefaultConstructor &&) noexcept = default;

        constexpr explicit EnableDefaultConstructor(EnableDefaultConstructorTag) {}
    };

    template<typename Tag>
    struct EnableDestructor<false, Tag>
    {
        ~EnableDestructor() noexcept = delete;
    };

    template<typename Tag>
    struct EnableCopyMove<false, true, true, true, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = delete;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = default;
    };

    template<typename Tag>
    struct EnableCopyMove<true, false, true, true, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = default;
    };

    template<typename Tag>
    struct EnableCopyMove<false, false, true, true, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = delete;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = default;
    };

    template<typename Tag>
    struct EnableCopyMove<true, true, false, true, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = default;
    };

    template<typename Tag>
    struct EnableCopyMove<false, true, false, true, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = delete;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = default;
    };

    template<typename Tag>
    struct EnableCopyMove<true, false, false, true, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = default;
    };

    template<typename Tag>
    struct EnableCopyMove<false, false, false, true, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = delete;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = default;
    };

    template<typename Tag>
    struct EnableCopyMove<true, true, true, false, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = delete;
    };

    template<typename Tag>
    struct EnableCopyMove<false, true, true, false, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = delete;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = delete;
    };

    template<typename Tag>
    struct EnableCopyMove<true, false, true, false, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = delete;
    };

    template<typename Tag>
    struct EnableCopyMove<false, false, true, false, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = delete;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = delete;
    };

    template<typename Tag>
    struct EnableCopyMove<true, true, false, false, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = delete;
    };

    template<typename Tag>
    struct EnableCopyMove<false, true, false, false, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = delete;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = default;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = delete;
    }; 

    template<typename Tag>
    struct EnableCopyMove<true, false, false, false, Tag>
    {
        constexpr EnableCopyMove() noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = default;

        constexpr EnableCopyMove(EnableCopyMove &&) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = delete;
    };

    template<typename Tag>
    struct EnableCopyMove<false, false, false, false, Tag>
    {
        constexpr EnableCopyMove() noexcept                       = default;
        constexpr EnableCopyMove(EnableCopyMove const &) noexcept = delete;
        constexpr EnableCopyMove(EnableCopyMove &&) noexcept      = delete;

        EnableCopyMove &operator=(EnableCopyMove const &) noexcept = delete;

        EnableCopyMove &operator=(EnableCopyMove &&) noexcept = delete;
    };
} // namespace SFTL
