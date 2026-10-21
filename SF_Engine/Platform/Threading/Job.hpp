#pragma once
#include <UtilityClasses/NoCopy.hpp>
#include <thread>

namespace SF::Engine
{
    using namespace std;
    class Job : NoCopy
    {
    public:
        Job(bool isAutoDelete, bool isCompletion = false, uint8_t priority = 0);
        enum State
        {
            STATE_COMPLETE_OR_CANCELLED, // Default to a cancelled state so if something searches for a job, and it
                                         // returns nothing, it is done or cancelled
            STATE_SETUP,
            STATE_STARTED,
            STATE_PENDING,
            STATE_PROCESSING,
            STATE_SUSPENDED
        };

        virtual ~Job() {}

        virtual void Reset(bool shouldClearDependent);
        virtual void Execute();

        void SetDependent(Job *dependent);
        void SetDependentStarted(Job *dependent);

        void SetContinuation(Job *continuationJob);

        void StartAsChild(Job *childJob);

        void WaitForChildren();
        [[nodiscard]] bool IsCancelled() const;
        [[nodiscard]] bool IsCompleted() const;
        [[nodiscard]] bool ShouldAutoDelete() const;

        void StartAndWaitForCompletion();

        [[nodiscard]] Job *GetDependent() const;

        [[nodiscard]] unsigned int GetDependentCount() const;
        void IncrementDependentCount();
        void DecrementDependentCount();

        [[nodiscard]] uint8_t GetPriority() const;

    private:
        enum
        {
            // flags
            FLAG_AUTO_DELETE = (1 << 31),
            FLAG_CHILD_JOBS  = (1 << 30),
            FLAG_COMPLETION =
                    (1 << 29), // Completion runs in place when it's dependency count is zero (no need to be scheduled)
            FLAG_RESERVED = (1 << 28), // Reserved, can be used in the future if needed

            // 8 bits for priority
            FLAG_PRIORITY_MASK      = 0x0ff00000,
            FLAG_PRIORITY_START_BIT = 20,

            // 20 bits for count
            FLAG_DEPENDENT_COUNT_MASK = 0x000fffff
        };
    };
} // namespace SF::Engine
