#pragma once

namespace SF::Engine
{
    /**
     * @brief Interface that disables copy operations while allowing move
     *
     * Inherit from this class to make a type move-only.
     * This is useful for resource-owning types that should not be copied.
     */
    class NoCopy
    {
    protected:
        constexpr NoCopy() noexcept = default;
        virtual ~NoCopy() = default;

    public:
        // Delete copy operations
        NoCopy(const NoCopy &) = delete;
        NoCopy &operator=(const NoCopy &) = delete;

        // Allow move operations
        NoCopy(NoCopy &&) noexcept = default;
        NoCopy &operator=(NoCopy &&) noexcept = default;
    };

    /**
     * @brief Interface that disables both copy and move operations
     *
     * Inherit from this class to make a type completely non-transferable.
     * Useful for singleton-like objects or types with complex invariants.
     */
    class NoTransfer
    {
    protected:
        constexpr NoTransfer() noexcept = default;
        virtual ~NoTransfer() = default;

    public:
        // Delete copy operations
        NoTransfer(const NoTransfer &) = delete;
        NoTransfer &operator=(const NoTransfer &) = delete;

        // Delete move operations
        NoTransfer(NoTransfer &&) = delete;
        NoTransfer &operator=(NoTransfer &&) = delete;
    };

    /**
     * @brief Interface that only allows copy operations (no move)
     *
     * Useful for types where move semantics don't provide benefits
     * or where copying is always necessary.
     */
    class NoMove
    {
    protected:
        constexpr NoMove() noexcept = default;
        virtual ~NoMove() = default;

    public:
        // Allow copy operations
        NoMove(const NoMove &) = default;
        NoMove &operator=(const NoMove &) = default;

        // Delete move operations
        NoMove(NoMove &&) = delete;
        NoMove &operator=(NoMove &&) = delete;
    };
}