/******************************************************************************/
/* Time.hpp                                                                   */
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

#include <Globals.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace SF::Engine
{
    using namespace std::chrono_literals;

    /**
     * @brief Concept for duration-like types
     */
    template<typename T>
    concept DurationLike = requires(T t) {
        { std::chrono::duration_cast<std::chrono::microseconds>(t) };
    };

    template<typename Period, typename Rep = int64_t>
    using Duration  = std::chrono::duration<Rep, Period>;
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    using DurationAsSeconds      = std::chrono::duration<double>; // Period defaults to ratio<1>
    using DurationAsMilliseconds = Duration<std::milli>;
    using DurationAsMicroseconds = Duration<std::micro>;
    using DurationAsNanoseconds  = Duration<std::nano>;


    /**
     * @brief Gets the current date/time as a formatted string
     * @param format The format string (uses std::put_time format)
     * @return The formatted date/time string
     */
    [[nodiscard]] static std::string GetDateTime(std::string_view format = "%Y-%m-%d %H:%M:%S")
    {
        auto now   = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);

        std::stringstream ss;
        ss << std::put_time(std::localtime(&timeT), format.data());
        return ss.str();
    }

    /**
     * @brief Gets the current time since application start (steady clock)
     * @return The current time
     */
    [[nodiscard]] inline TimePoint ClockNow() noexcept
    {
        static const auto s_epoch = Clock::now();
        return TimePoint(Clock::now() - s_epoch);
    }

    /**
     * @brief Creates a time value from seconds
     * @param seconds Number of seconds
     * @return DurationAsSeconds value constructed from seconds
     */
    template<typename Rep = double>
    [[nodiscard]] static constexpr DurationAsSeconds Seconds(Rep seconds) noexcept
    {
        return DurationAsSeconds(seconds);
    }

    /**
     * @brief Creates a time value from milliseconds
     * @param milliseconds Number of milliseconds
     * @return DurationAsSeconds value constructed from milliseconds
     */
    template<typename Rep = int32_t>
    [[nodiscard]] static constexpr DurationAsMilliseconds Milliseconds(Rep milliseconds) noexcept
    {
        return std::chrono::round<DurationAsMilliseconds>(std::chrono::duration<Rep, std::milli>(milliseconds));
    }

    /**
     * @brief Creates a time value from microseconds
     * @param microseconds Number of microseconds
     * @return DurationAsSeconds value constructed from microseconds
     */
    template<typename Rep = int64_t>
    [[nodiscard]] static constexpr DurationAsMicroseconds Microseconds(Rep microseconds) noexcept
    {
        return std::chrono::round<DurationAsMicroseconds>(std::chrono::duration<Rep, std::micro>(microseconds));
    }

    /**
     * @brief Creates a time value from nanoseconds
     * @param nanoseconds Number of nanoseconds
     * @return DurationAsSeconds value constructed from nanoseconds
     */
    template<typename Rep = int64_t>
    [[nodiscard]] static constexpr DurationAsNanoseconds Nanoseconds(Rep nanoseconds) noexcept
    {
        return std::chrono::round<DurationAsNanoseconds>(std::chrono::duration<Rep, std::nano>(nanoseconds));
    }

    /**
     * @brief Represents a time value stored in microseconds with high precision
     */

    /**
     * @brief Tracks elapsed time and counts intervals
     */
    class ElapsedTime
    {
    public:
        ElapsedTime() noexcept : ElapsedTime(Nanoseconds(-1)) {}

        template<typename Rep, typename Period>
        explicit ElapsedTime(const std::chrono::duration<Rep, Period> &interval) noexcept :
            m_startTime(Clock::now()), m_interval(std::chrono::round<DurationAsNanoseconds>(interval))
        {
        }

        uint32_t GetElapsed() noexcept
        {
            const auto now = Clock::now();

            if (m_interval.count() <= 0)
            {
                m_startTime = now;
                return 1;
            }

            const auto elapsed = static_cast<uint32_t>((now - m_startTime) / m_interval);
            if (elapsed > 0)
                m_startTime = now;

            return elapsed;
        }

        [[nodiscard]] DurationAsNanoseconds GetElapsedTime() const noexcept { return Clock::now() - m_startTime; }

        bool HasElapsed() noexcept { return GetElapsed() > 0; }

        void Reset() noexcept { m_startTime = Clock::now(); }

        [[nodiscard]] TimePoint GetStartTime() const noexcept { return m_startTime; }
        void SetStartTime(const TimePoint &startTime) noexcept { m_startTime = startTime; }
        [[nodiscard]] const DurationAsNanoseconds &GetInterval() const noexcept { return m_interval; }

        template<typename Rep, typename Period>
        void SetInterval(const std::chrono::duration<Rep, Period> &interval) noexcept
        {
            m_interval = std::chrono::round<DurationAsNanoseconds>(interval);
        }

    private:
        TimePoint m_startTime;
        DurationAsNanoseconds m_interval;
    };

    /**
     * @brief RAII timer for profiling code sections
     */
    class ScopedTimer
    {
    public:
        using CallbackFn = std::function<void(TimePoint)>;

        explicit ScopedTimer(CallbackFn callback) noexcept : m_callback(std::move(callback)), m_start(Clock::now()) {}

        explicit ScopedTimer(TimePoint &result) noexcept : m_result(&result), m_start(Clock::now()) {}

        ~ScopedTimer()
        {
            TimePoint elapsed = TimePoint(Clock::now() - m_start);

            if (m_callback)
                m_callback(elapsed);

            if (m_result)
                *m_result = elapsed;
        }

        ScopedTimer(const ScopedTimer &)            = delete;
        ScopedTimer &operator=(const ScopedTimer &) = delete;
        ScopedTimer(ScopedTimer &&)                 = delete;
        ScopedTimer &operator=(ScopedTimer &&)      = delete;

    private:
        CallbackFn m_callback;
        TimePoint *m_result = nullptr;
        TimePoint m_start;
    };

    /**
     * @brief Stopwatch for manual timing control
     */
    class Stopwatch
    {
    public:
        Stopwatch() = default;

        void Start() noexcept
        {
            if (!m_running)
            {
                m_running   = true;
                m_startTime = Clock::now();
            }
        }

        void Stop() noexcept
        {
            if (m_running)
            {
                m_running = false;
                m_elapsed += Clock::now() - m_startTime;
            }
        }

        void Reset() noexcept
        {
            m_running   = false;
            m_elapsed   = DurationAsNanoseconds(0);
            m_startTime = TimePoint(DurationAsNanoseconds(0));
        }

        void Restart() noexcept
        {
            Reset();
            Start();
        }

        [[nodiscard]] DurationAsNanoseconds GetElapsed() const noexcept
        {
            if (m_running)
                return m_elapsed + (Clock::now() - m_startTime);
            return m_elapsed;
        }

        [[nodiscard]] bool IsRunning() const noexcept { return m_running; }

    private:
        bool m_running                  = false;
        DurationAsNanoseconds m_elapsed = DurationAsNanoseconds(0);
        TimePoint m_startTime           = TimePoint(DurationAsNanoseconds(0));
    };

    /**
     * @brief Simple FPS counter
     */
    class FPSCounter
    {
    public:
        explicit FPSCounter(const DurationAsNanoseconds &updateInterval = DurationAsNanoseconds(1s)) noexcept :
            m_updateInterval(updateInterval), m_lastUpdate(Clock::now())
        {
        }

        void Update() noexcept
        {
            m_frameCount++;
            const auto now     = Clock::now();
            const auto elapsed = now - m_lastUpdate;

            if (elapsed >= m_updateInterval)
            {
                m_fps        = static_cast<double>(m_frameCount) / std::chrono::duration<double>(elapsed).count();
                m_frameCount = 0;
                m_lastUpdate = now;
            }
        }

        [[nodiscard]] double GetFPS() const noexcept { return m_fps; }

        [[nodiscard]] double GetFrameTime() const noexcept { return m_fps > 0.0 ? 1000.0 / m_fps : 0.0; }

    private:
        DurationAsNanoseconds m_updateInterval;
        TimePoint m_lastUpdate;
        uint32_t m_frameCount = 0;
        double m_fps          = 0.0;
    };

    class DeltaTime
    {
    public:
        void Update()
        {
            currentFrameTime = Clock::now();
            change           = currentFrameTime - lastFrameTime;
            lastFrameTime    = currentFrameTime;
        }

        TimePoint currentFrameTime;
        TimePoint lastFrameTime;
        DurationAsNanoseconds change;
    };

    template<typename ClockType = std::chrono::steady_clock>
    class UpdateClock
    {
    public:
        using clock      = ClockType;
        using time_point = clock::time_point;
        using duration   = clock::duration;

        uint32_t value_ = 0;

        static_assert(std::chrono::is_clock_v<ClockType>, "ClockType must satisfy the Clock concept");

        using RateCallback = std::function<void(uint32_t rate)>;

        std::optional<uint32_t> Update(const time_point &time = clock::now())
        {
            ++valueTemp_;

            const auto currentSeconds = std::chrono::floor<std::chrono::seconds>(time);
            const auto lastSeconds    = std::chrono::floor<std::chrono::seconds>(valueTime_);

            if (currentSeconds > lastSeconds)
            {
                value_     = std::exchange(valueTemp_, 0);
                valueTime_ = time;

                if (rateCallback_)
                {
                    rateCallback_(value_);
                }

                return value_;
            }

            valueTime_ = time;
            return std::nullopt;
        }

        std::optional<uint32_t> Update(const DurationAsNanoseconds &time)
        {
            return Update(time_point{} + std::chrono::duration_cast<duration>(time));
        }

        [[nodiscard]] constexpr uint32_t GetRate() const noexcept { return value_; }

        [[nodiscard]] constexpr uint32_t GetCurrentPartialCount() const noexcept { return valueTemp_; }

        [[nodiscard]] duration GetElapsedInCurrentSecond(const time_point &currentTime = clock::now()) const
        {
            const auto currentSeconds = std::chrono::floor<std::chrono::seconds>(currentTime);
            return currentTime - currentSeconds;
        }

        [[nodiscard]] duration GetRemainingInCurrentSecond(const time_point &currentTime = clock::now()) const
        {
            constexpr auto oneSecond = std::chrono::seconds(1);
            return oneSecond - GetElapsedInCurrentSecond(currentTime);
        }

        [[nodiscard]] double GetProjectedRate(const time_point &currentTime = clock::now()) const
        {
            if (valueTemp_ == 0)
                return 0.0;

            const auto elapsed = GetElapsedInCurrentSecond(currentTime);
            if (elapsed.count() == 0)
                return 0.0;

            const double secondsElapsed = std::chrono::duration<double>(elapsed).count();
            return static_cast<double>(valueTemp_) / secondsElapsed;
        }

        void Reset() noexcept
        {
            value_     = 0;
            valueTemp_ = 0;
            valueTime_ = time_point{};
        }

        void SetRateCallback(RateCallback callback) { rateCallback_ = std::move(callback); }

        [[nodiscard]] constexpr operator uint32_t() const noexcept { return value_; }

        [[nodiscard]] auto MakeScopedUpdater() { return ScopedUpdater(*this); }

    private:
        uint32_t valueTemp_ = 0;
        time_point valueTime_;
        RateCallback rateCallback_;

        class ScopedUpdater
        {
        public:
            explicit ScopedUpdater(UpdateClock &counter) : counter_(counter) {}

            ~ScopedUpdater() { counter_.Update(); }

            ScopedUpdater(const ScopedUpdater &)                = delete;
            ScopedUpdater &operator=(const ScopedUpdater &)     = delete;
            ScopedUpdater(ScopedUpdater &&) noexcept            = default;
            ScopedUpdater &operator=(ScopedUpdater &&) noexcept = default;

        private:
            UpdateClock &counter_;
        };
    };

    template<typename ClockType = std::chrono::steady_clock>
    class RateTracker : public UpdateClock<ClockType>
    {
    public:
        using Base = UpdateClock<ClockType>;
        using typename Base::time_point;

        struct Statistics
        {
            uint32_t minRate     = 0;
            uint32_t maxRate     = 0;
            double averageRate   = 0.0;
            uint32_t sampleCount = 0;
        };

        std::optional<uint32_t> Update(const time_point &time = Base::clock::now())
        {
            auto result = Base::Update(time);

            if (result)
            {
                UpdateStatistics(*result);
            }

            return result;
        }

        [[nodiscard]] const Statistics &GetStatistics() const noexcept { return stats_; }

        void ResetStatistics() noexcept { stats_ = Statistics{}; }

    private:
        void UpdateStatistics(uint32_t newRate)
        {
            if (stats_.sampleCount == 0)
            {
                stats_.minRate     = newRate;
                stats_.maxRate     = newRate;
                stats_.averageRate = static_cast<double>(newRate);
            } else
            {
                stats_.minRate = std::min(stats_.minRate, newRate);
                stats_.maxRate = std::max(stats_.maxRate, newRate);

                const double total = stats_.averageRate * stats_.sampleCount + newRate;
                stats_.averageRate = total / (stats_.sampleCount + 1);
            }

            ++stats_.sampleCount;
        }

        Statistics stats_;
    };

    template<typename ClockType>
    UpdateClock(ClockType) -> UpdateClock<ClockType>;

} // namespace SF::Engine
#pragma GCC diagnostic pop
