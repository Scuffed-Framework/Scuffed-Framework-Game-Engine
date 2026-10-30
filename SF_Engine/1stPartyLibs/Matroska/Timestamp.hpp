#pragma once

#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace SF::Matroska
{
    using namespace std;
    class Timestamp
    {
        uint64_t timecodeScale_ = 1000000; // Default: 1ms = 1,000,000 ns
        chrono::nanoseconds baseTime_{0};
        chrono::nanoseconds clusterStart_{0};

    public:
        Timestamp() = default;
        explicit Timestamp(uint64_t timecodeScale) : timecodeScale_(timecodeScale) {}

        void set_timecode_scale(uint64_t scale) { timecodeScale_ = scale; }

        [[nodiscard]] uint64_t timecode_scale() const { return timecodeScale_; }

        // Convert from nanoseconds to timecode (cluster-relative)
        [[nodiscard]] uint64_t to_timecode(chrono::nanoseconds absolute) const
        {
            auto relative = absolute - clusterStart_;
            return static_cast<uint64_t>(relative.count() / timecodeScale_);
        }

        // Convert from timecode to nanoseconds (cluster-relative)
        [[nodiscard]] chrono::nanoseconds from_timecode(uint64_t timecode) const
        {
            return clusterStart_ + chrono::nanoseconds(timecode * timecodeScale_);
        }

        // Get cluster-relative timecode (int16_t for Blocks)
        [[nodiscard]] int16_t block_timecode(chrono::nanoseconds absolute) const
        {
            auto diff   = absolute - clusterStart_;
            auto scaled = diff.count() / static_cast<int64_t>(timecodeScale_);
            if (scaled > 32767 || scaled < -32768)
            {
                throw runtime_error("Block timecode out of range (-32768 to 32767)");
            }
            return static_cast<int16_t>(scaled);
        }

        // Get absolute time from block timecode
        [[nodiscard]] chrono::nanoseconds from_block_timecode(int16_t timecode) const
        {
            return clusterStart_ + chrono::nanoseconds(static_cast<int64_t>(timecode) * timecodeScale_);
        }

        void set_cluster_start(chrono::nanoseconds start) { clusterStart_ = start; }

        [[nodiscard]] chrono::nanoseconds cluster_start() const { return clusterStart_; }
    };

    // Duration helper
    inline uint64_t duration_to_timecode(chrono::nanoseconds duration, uint64_t timecodeScale)
    {
        return static_cast<uint64_t>(duration.count() / timecodeScale);
    }

    inline chrono::nanoseconds timecode_to_duration(uint64_t timecode, uint64_t timecodeScale)
    {
        return chrono::nanoseconds(timecode * timecodeScale);
    }

} // namespace SF::Matroska
