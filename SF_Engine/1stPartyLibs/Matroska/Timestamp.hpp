#pragma once

#include <chrono>
#include <cstdint>

namespace SF::Matroska
{

    class Timestamp
    {
    private:
        uint64_t timecodeScale_ = 1000000; // Default: 1ms = 1,000,000 ns
        std::chrono::nanoseconds baseTime_{0};
        std::chrono::nanoseconds clusterStart_{0};

    public:
        Timestamp() = default;
        explicit Timestamp(uint64_t timecodeScale) : timecodeScale_(timecodeScale) {}

        void set_timecode_scale(uint64_t scale)
        {
            timecodeScale_ = scale;
        }

        uint64_t timecode_scale() const
        {
            return timecodeScale_;
        }

        // Convert from nanoseconds to timecode (cluster-relative)
        uint64_t to_timecode(std::chrono::nanoseconds absolute) const
        {
            auto relative = absolute - clusterStart_;
            return static_cast<uint64_t>(relative.count() / timecodeScale_);
        }

        // Convert from timecode to nanoseconds (cluster-relative)
        std::chrono::nanoseconds from_timecode(uint64_t timecode) const
        {
            return clusterStart_ + std::chrono::nanoseconds(timecode * timecodeScale_);
        }

        // Get cluster-relative timecode (int16_t for Blocks)
        int16_t block_timecode(std::chrono::nanoseconds absolute) const
        {
            auto diff = absolute - clusterStart_;
            auto scaled = diff.count() / static_cast<int64_t>(timecodeScale_);
            if (scaled > 32767 || scaled < -32768)
            {
                throw std::runtime_error("Block timecode out of range (-32768 to 32767)");
            }
            return static_cast<int16_t>(scaled);
        }

        // Get absolute time from block timecode
        std::chrono::nanoseconds from_block_timecode(int16_t timecode) const
        {
            return clusterStart_ + std::chrono::nanoseconds(
                                       static_cast<int64_t>(timecode) * timecodeScale_);
        }

        void set_cluster_start(std::chrono::nanoseconds start)
        {
            clusterStart_ = start;
        }

        std::chrono::nanoseconds cluster_start() const
        {
            return clusterStart_;
        }
    };

    // Duration helper
    inline uint64_t duration_to_timecode(std::chrono::nanoseconds duration, uint64_t timecodeScale)
    {
        return static_cast<uint64_t>(duration.count() / timecodeScale);
    }

    inline std::chrono::nanoseconds timecode_to_duration(uint64_t timecode, uint64_t timecodeScale)
    {
        return std::chrono::nanoseconds(timecode * timecodeScale);
    }

} // namespace SF::EBML::Matroska