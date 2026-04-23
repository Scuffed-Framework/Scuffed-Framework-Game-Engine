/******************************************************************************/
/* Time.hpp                                                                   */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Martin.                                         */
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

#include <chrono>
#include <string>
#include <iomanip>
#include <sstream>
#include <concepts>
#include <functional>
#include <cmath>

namespace SF::Engine
{
    using namespace std::chrono_literals;

    /**
     * @brief Concept for duration-like types
     */
    template <typename T>
    concept DurationLike = requires(T t) {
        { std::chrono::duration_cast<std::chrono::microseconds>(t) };
    };

    /**
     * @brief Represents a time value stored in microseconds with high precision
     */
    class Time
    {
    public:
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::microseconds;

        constexpr Time() noexcept = default;

        /**
         * @brief Creates a new time from a chrono duration
         * @param duration The duration
         */
        template <typename Rep, typename Period>
        constexpr Time(const std::chrono::duration<Rep, Period> &duration) noexcept
            : m_value(std::chrono::duration_cast<Duration>(duration))
        {
        }

        /**
         * @brief Creates a time value from seconds
         * @param seconds Number of seconds
         * @return Time value constructed from seconds
         */
        template <typename Rep = double>
        [[nodiscard]] static constexpr Time Seconds(Rep seconds) noexcept
        {
            return Time(std::chrono::duration<Rep>(seconds));
        }

        /**
         * @brief Creates a time value from milliseconds
         * @param milliseconds Number of milliseconds
         * @return Time value constructed from milliseconds
         */
        template <typename Rep = int32_t>
        [[nodiscard]] static constexpr Time Milliseconds(Rep milliseconds) noexcept
        {
            return Time(std::chrono::duration<Rep, std::milli>(milliseconds));
        }

        /**
         * @brief Creates a time value from microseconds
         * @param microseconds Number of microseconds
         * @return Time value constructed from microseconds
         */
        template <typename Rep = int64_t>
        [[nodiscard]] static constexpr Time Microseconds(Rep microseconds) noexcept
        {
            return Time(std::chrono::duration<Rep, std::micro>(microseconds));
        }

        /**
         * @brief Creates a time value from nanoseconds
         * @param nanoseconds Number of nanoseconds
         * @return Time value constructed from nanoseconds
         */
        template <typename Rep = int64_t>
        [[nodiscard]] static constexpr Time Nanoseconds(Rep nanoseconds) noexcept
        {
            return Time(std::chrono::duration<Rep, std::nano>(nanoseconds));
        }

        /**
         * @brief Gets the time value as seconds
         * @return Time in seconds
         */
        template <typename T = double>
        [[nodiscard]] constexpr T AsSeconds() const noexcept
        {
            return std::chrono::duration<T>(m_value).count();
        }

        /**
         * @brief Gets the time value as milliseconds
         * @return Time in milliseconds
         */
        template <typename T = double>
        [[nodiscard]] constexpr T AsMilliseconds() const noexcept
        {
            return std::chrono::duration<T, std::milli>(m_value).count();
        }

        /**
         * @brief Gets the time value as microseconds
         * @return Time in microseconds
         */
        template <typename T = int64_t>
        [[nodiscard]] constexpr T AsMicroseconds() const noexcept
        {
            return std::chrono::duration_cast<std::chrono::duration<T, std::micro>>(m_value).count();
        }

        /**
         * @brief Gets the time value as nanoseconds
         * @return Time in nanoseconds
         */
        template <typename T = int64_t>
        [[nodiscard]] constexpr T AsNanoseconds() const noexcept
        {
            return std::chrono::duration_cast<std::chrono::duration<T, std::nano>>(m_value).count();
        }

        /**
         * @brief Gets the underlying duration
         * @return The duration value
         */
        [[nodiscard]] constexpr Duration GetDuration() const noexcept
        {
            return m_value;
        }

        /**
         * @brief Checks if this time is zero
         * @return True if zero
         */
        [[nodiscard]] constexpr bool IsZero() const noexcept
        {
            return m_value == Duration::zero();
        }

        /**
         * @brief Checks if this time is negative
         * @return True if negative
         */
        [[nodiscard]] constexpr bool IsNegative() const noexcept
        {
            return m_value < Duration::zero();
        }

        /**
         * @brief Checks if this time is positive
         * @return True if positive
         */
        [[nodiscard]] constexpr bool IsPositive() const noexcept
        {
            return m_value > Duration::zero();
        }

        /**
         * @brief Gets the absolute value of this time
         * @return The absolute time
         */
        [[nodiscard]] constexpr Time Abs() const noexcept
        {
            return Time(m_value < Duration::zero() ? -m_value : m_value);
        }

        /**
         * @brief Modulo operation with another time
         * @param other The divisor
         * @return The remainder
         */
        [[nodiscard]] constexpr Time Mod(const Time &other) const noexcept
        {
            return Time(Duration(m_value.count() % other.m_value.count()));
        }

        /**
         * @brief Gets the current time since application start (steady clock)
         * @return The current time
         */
        [[nodiscard]] static Time Now() noexcept
        {
            static const auto s_epoch = Clock::now();
            return std::chrono::duration_cast<Duration>(Clock::now() - s_epoch);
        }

        /**
         * @brief Gets the current system time (wall clock)
         * @return The current system time
         */
        [[nodiscard]] static Time SystemNow() noexcept
        {
            auto now = std::chrono::system_clock::now();
            return std::chrono::duration_cast<Duration>(now.time_since_epoch());
        }

        /**
         * @brief Gets the current date/time as a formatted string
         * @param format The format string (uses std::put_time format)
         * @return The formatted date/time string
         */
        [[nodiscard]] static std::string GetDateTime(std::string_view format = "%Y-%m-%d %H:%M:%S")
        {
            auto now = std::chrono::system_clock::now();
            auto timeT = std::chrono::system_clock::to_time_t(now);

            std::stringstream ss;
            ss << std::put_time(std::localtime(&timeT), format.data());
            return ss.str();
        }

        /**
         * @brief Formats this time duration as a human-readable string
         * @return Formatted time string (e.g., "1.234s" or "123.4ms")
         */
        [[nodiscard]] std::string ToString() const
        {
            auto abs_us = std::abs(m_value.count());
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3);

            if (abs_us >= 1'000'000) // >= 1 second
            {
                oss << AsSeconds() << "s";
            }
            else if (abs_us >= 1'000) // >= 1 millisecond
            {
                oss << AsMilliseconds() << "ms";
            }
            else // < 1 millisecond
            {
                oss << m_value.count() << "μs";
            }

            return oss.str();
        }

        /**
         * @brief Convert to any chrono duration type
         */
        template <typename Rep, typename Period>
        [[nodiscard]] constexpr operator std::chrono::duration<Rep, Period>() const noexcept
        {
            return std::chrono::duration_cast<std::chrono::duration<Rep, Period>>(m_value);
        }

        // Comparison operators
        [[nodiscard]] constexpr bool operator==(const Time &rhs) const noexcept = default;
        [[nodiscard]] constexpr auto operator<=>(const Time &rhs) const noexcept = default;

        // Unary operators
        [[nodiscard]] constexpr Time operator-() const noexcept;
        [[nodiscard]] constexpr Time operator+() const noexcept;

        // Arithmetic operators
        friend constexpr Time operator+(const Time &lhs, const Time &rhs) noexcept;
        friend constexpr Time operator-(const Time &lhs, const Time &rhs) noexcept;

        template <std::floating_point T>
        friend constexpr Time operator*(const Time &lhs, T rhs) noexcept;

        template <std::integral T>
        friend constexpr Time operator*(const Time &lhs, T rhs) noexcept;

        template <std::floating_point T>
        friend constexpr Time operator*(T lhs, const Time &rhs) noexcept;

        template <std::integral T>
        friend constexpr Time operator*(T lhs, const Time &rhs) noexcept;

        template <std::floating_point T>
        friend constexpr Time operator/(const Time &lhs, T rhs) noexcept;

        template <std::integral T>
        friend constexpr Time operator/(const Time &lhs, T rhs) noexcept;

        friend constexpr double operator/(const Time &lhs, const Time &rhs) noexcept;

        // Compound assignment operators
        constexpr Time &operator+=(const Time &rhs) noexcept;
        constexpr Time &operator-=(const Time &rhs) noexcept;

        template <typename T>
        constexpr Time &operator*=(T rhs) noexcept;

        template <typename T>
        constexpr Time &operator/=(T rhs) noexcept;

        // Stream operator
        friend std::ostream &operator<<(std::ostream &os, const Time &time);

    private:
        Duration m_value{};
    };

    /**
     * @brief Tracks elapsed time and counts intervals
     */
    class ElapsedTime
    {
    public:
        explicit ElapsedTime(const Time &interval = Time::Seconds(-1)) noexcept
            : m_startTime(Time::Now()), m_interval(interval)
        {
        }

        uint32_t GetElapsed() noexcept
        {
            auto now = Time::Now();
            auto elapsed = static_cast<uint32_t>((now - m_startTime) / m_interval);

            if (elapsed > 0)
                m_startTime = now;

            return elapsed;
        }

        [[nodiscard]] Time GetElapsedTime() const noexcept
        {
            return Time::Now() - m_startTime;
        }

        bool HasElapsed() noexcept
        {
            return GetElapsed() > 0;
        }

        void Reset() noexcept
        {
            m_startTime = Time::Now();
        }

        [[nodiscard]] const Time &GetStartTime() const noexcept { return m_startTime; }
        void SetStartTime(const Time &startTime) noexcept { m_startTime = startTime; }
        [[nodiscard]] const Time &GetInterval() const noexcept { return m_interval; }
        void SetInterval(const Time &interval) noexcept { m_interval = interval; }

    private:
        Time m_startTime;
        Time m_interval;
    };

    /**
     * @brief RAII timer for profiling code sections
     */
    class ScopedTimer
    {
    public:
        using CallbackFn = std::function<void(Time)>;

        explicit ScopedTimer(CallbackFn callback) noexcept
            : m_callback(std::move(callback)), m_start(Time::Now())
        {
        }

        explicit ScopedTimer(Time &result) noexcept
            : m_result(&result), m_start(Time::Now())
        {
        }

        ~ScopedTimer()
        {
            auto elapsed = Time::Now() - m_start;

            if (m_callback)
                m_callback(elapsed);

            if (m_result)
                *m_result = elapsed;
        }

        ScopedTimer(const ScopedTimer &) = delete;
        ScopedTimer &operator=(const ScopedTimer &) = delete;
        ScopedTimer(ScopedTimer &&) = delete;
        ScopedTimer &operator=(ScopedTimer &&) = delete;

    private:
        CallbackFn m_callback;
        Time *m_result = nullptr;
        Time m_start;
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
                m_running = true;
                m_startTime = Time::Now();
            }
        }

        void Stop() noexcept
        {
            if (m_running)
            {
                m_running = false;
                m_elapsed += Time::Now() - m_startTime;
            }
        }

        void Reset() noexcept
        {
            m_running = false;
            m_elapsed = Time::Seconds(0);
            m_startTime = Time::Seconds(0);
        }

        void Restart() noexcept
        {
            Reset();
            Start();
        }

        [[nodiscard]] Time GetElapsed() const noexcept
        {
            if (m_running)
                return m_elapsed + (Time::Now() - m_startTime);
            return m_elapsed;
        }

        [[nodiscard]] bool IsRunning() const noexcept { return m_running; }

    private:
        bool m_running = false;
        Time m_elapsed = Time::Seconds(0);
        Time m_startTime = Time::Seconds(0);
    };

    /**
     * @brief Simple FPS counter
     */
    class FPSCounter
    {
    public:
        explicit FPSCounter(const Time &updateInterval = Time::Seconds(1)) noexcept
            : m_updateInterval(updateInterval), m_lastUpdate(Time::Now())
        {
        }

        void Update() noexcept
        {
            m_frameCount++;
            auto now = Time::Now();
            auto elapsed = now - m_lastUpdate;

            if (elapsed >= m_updateInterval)
            {
                m_fps = static_cast<double>(m_frameCount) / elapsed.AsSeconds();
                m_frameCount = 0;
                m_lastUpdate = now;
            }
        }

        [[nodiscard]] double GetFPS() const noexcept { return m_fps; }

        [[nodiscard]] double GetFrameTime() const noexcept
        {
            return m_fps > 0.0 ? 1000.0 / m_fps : 0.0;
        }

    private:
        Time m_updateInterval;
        Time m_lastUpdate;
        uint32_t m_frameCount = 0;
        double m_fps = 0.0;
    };

    class DeltaTime
    {
    public:
        void Update()
        {
            currentFrameTime = Time::Now();
            change = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;
        }

        Time currentFrameTime;
        Time lastFrameTime;
        Time change;
    };

    template <typename ClockType = std::chrono::steady_clock>
    class UpdatesPerSecond
    {
    public:
        using clock = ClockType;
        using time_point = typename clock::time_point;
        using duration = typename clock::duration;

        uint32_t value_ = 0;

        static_assert(std::chrono::is_clock_v<ClockType>, "ClockType must satisfy the Clock concept");

        using RateCallback = std::function<void(uint32_t rate)>;

        std::optional<uint32_t> Update(const time_point &time = clock::now())
        {
            ++valueTemp_;

            const auto currentSeconds = std::chrono::floor<std::chrono::seconds>(time);
            const auto lastSeconds = std::chrono::floor<std::chrono::seconds>(valueTime_);

            if (currentSeconds > lastSeconds)
            {
                value_ = std::exchange(valueTemp_, 0);
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

        std::optional<uint32_t> Update(const Time &time)
        {
            const auto seconds = std::chrono::duration_cast<duration>(
                std::chrono::duration<double>(time.AsSeconds()));
            return Update(time_point{} + seconds);
        }

        [[nodiscard]] constexpr uint32_t GetRate() const noexcept
        {
            return value_;
        }

        [[nodiscard]] constexpr uint32_t GetCurrentPartialCount() const noexcept
        {
            return valueTemp_;
        }

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
            value_ = 0;
            valueTemp_ = 0;
            valueTime_ = time_point{};
        }

        void SetRateCallback(RateCallback callback)
        {
            rateCallback_ = std::move(callback);
        }

        [[nodiscard]] constexpr operator uint32_t() const noexcept
        {
            return value_;
        }

        [[nodiscard]] auto MakeScopedUpdater()
        {
            return ScopedUpdater(*this);
        }

    private:
        uint32_t valueTemp_ = 0;
        time_point valueTime_;
        RateCallback rateCallback_;

        class ScopedUpdater
        {
        public:
            explicit ScopedUpdater(UpdatesPerSecond &counter) : counter_(counter) {}

            ~ScopedUpdater()
            {
                counter_.Update();
            }

            ScopedUpdater(const ScopedUpdater &) = delete;
            ScopedUpdater &operator=(const ScopedUpdater &) = delete;
            ScopedUpdater(ScopedUpdater &&) noexcept = default;
            ScopedUpdater &operator=(ScopedUpdater &&) noexcept = default;

        private:
            UpdatesPerSecond &counter_;
        };
    };

    template <typename ClockType = std::chrono::steady_clock>
    class RateTracker : public UpdatesPerSecond<ClockType>
    {
    public:
        using Base = UpdatesPerSecond<ClockType>;
        using typename Base::time_point;

        struct Statistics
        {
            uint32_t minRate = 0;
            uint32_t maxRate = 0;
            double averageRate = 0.0;
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

        [[nodiscard]] const Statistics &GetStatistics() const noexcept
        {
            return stats_;
        }

        void ResetStatistics() noexcept
        {
            stats_ = Statistics{};
        }

    private:
        void UpdateStatistics(uint32_t newRate)
        {
            if (stats_.sampleCount == 0)
            {
                stats_.minRate = newRate;
                stats_.maxRate = newRate;
                stats_.averageRate = static_cast<double>(newRate);
            }
            else
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

    template <typename ClockType>
    UpdatesPerSecond(ClockType) -> UpdatesPerSecond<ClockType>;

}

#include "Time.inl"