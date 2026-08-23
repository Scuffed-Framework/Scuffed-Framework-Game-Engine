#pragma once

#include <cassert>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string_view>

#include <Math/Time/Time.hpp>

#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace SF::Engine
{
    /**
     * @brief A logging class used in Engine, will write output to the standard stream and into a
     * file.
     */
    class Log
    {
    public:
        // Color/style constants (compatible with spdlog's pattern formatters)
        struct Styles
        {
            constexpr static std::string_view Reset = "\033[0m";
            constexpr static std::string_view Bold = "\033[1m";
            constexpr static std::string_view Dim = "\033[2m";
            constexpr static std::string_view Underlined = "\033[4m";
            constexpr static std::string_view Blink = "\033[5m";
            constexpr static std::string_view Reverse = "\033[7m";
            constexpr static std::string_view Hidden = "\033[8m";
        };

        struct Colours
        {
            constexpr static std::string_view Default = "\033[0m";

            constexpr static std::string_view Black = "\033[30m";
            constexpr static std::string_view Red = "\033[31m";
            constexpr static std::string_view Green = "\033[32m";
            constexpr static std::string_view Yellow = "\033[33m";
            constexpr static std::string_view Blue = "\033[34m";
            constexpr static std::string_view Magenta = "\033[35m";
            constexpr static std::string_view Cyan = "\033[36m";
            constexpr static std::string_view LightGrey = "\033[37m";

            constexpr static std::string_view DarkGrey = "\033[90m";
            constexpr static std::string_view LightRed = "\033[91m";
            constexpr static std::string_view LightGreen = "\033[92m";
            constexpr static std::string_view LightYellow = "\033[93m";
            constexpr static std::string_view LightBlue = "\033[94m";
            constexpr static std::string_view LightMagenta = "\033[95m";
            constexpr static std::string_view LightCyan = "\033[96m";
            constexpr static std::string_view White = "\033[97m";
        };

        constexpr static std::string_view TimestampFormat = "%H:%M:%S";

        /**
         * Initialize the logging system with default sinks (console and file)
         * @param filepath Path to the log file
         * @param name Logger name (default: "Engine")
         */
        static void Init(const std::filesystem::path &filepath = "logs/Engine.log",
                         const std::string &name = "Engine");

        /**
         * Shutdown the logging system
         */
        static void Shutdown();

        /**
         * Get the main logger instance
         */
        static std::shared_ptr<spdlog::logger> &GetLogger();

        /**
         * Outputs a message into the console.
         * Uses spdlog format strings: Log::Out("Value: {}", myValue);
         */
        template <typename... Args>
        static void Out(fmt::format_string<Args...> fmt, Args &&...args)
        {
            auto &loggerRef = GetLogger();
            if (loggerRef)
            {
                loggerRef->info(fmt, std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a debug message into the console.
         * Uses spdlog format strings: Log::Debug("Value: {}", myValue);
         */
        template <typename... Args>
        static void Debug(fmt::format_string<Args...> fmt, Args &&...args)
        {
            auto &loggerRef = GetLogger();
            if (loggerRef)
            {
                loggerRef->debug(fmt, std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a info message into the console.
         * Uses spdlog format strings: Log::Info("Value: {}", myValue);
         */
        template <typename... Args>
        static void Info(fmt::format_string<Args...> fmt, Args &&...args)
        {
            auto &loggerRef = GetLogger();
            if (loggerRef)
            {
                loggerRef->info(fmt, std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a warning message into the console.
         * Uses spdlog format strings: Log::Warning("Value: {}", myValue);
         */
        template <typename... Args>
        static void Warning(fmt::format_string<Args...> fmt, Args &&...args)
        {
            auto &loggerRef = GetLogger();
            if (loggerRef)
            {
                loggerRef->warn(fmt, std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a error message into the console.
         * Uses spdlog format strings: Log::Error("Value: {}", myValue);
         */
        template <typename... Args>
        static void Error(fmt::format_string<Args...> fmt, Args &&...args)
        {
            auto &loggerRef = GetLogger();
            if (loggerRef)
            {
                loggerRef->error(fmt, std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs a critical message into the console.
         * Uses spdlog format strings: Log::Critical("Value: {}", myValue);
         */
        template <typename... Args>
        static void Critical(fmt::format_string<Args...> fmt, Args &&...args)
        {
            auto &loggerRef = GetLogger();
            if (loggerRef)
            {
                loggerRef->critical(fmt, std::forward<Args>(args)...);
            }
        }

        /**
         * Outputs an assert message into the console.
         * @param expr The expression to assertion check.
         * @param fmt Format string
         * @param args Format arguments
         */
        template <typename... Args>
        static void Assert(bool expr, fmt::format_string<Args...> fmt, Args &&...args)
        {
            if (!expr)
            {
                auto &loggerRef = GetLogger();
                if (loggerRef)
                {
                    loggerRef->critical("Assertion failed: {}",
                                        fmt::format(fmt, std::forward<Args>(args)...));
                }
                assert(false);
            }
        }

        /**
         * Sets the log level for the logger
         * @param level The spdlog level to set
         */
        static void SetLevel(spdlog::level::level_enum level);

        /**
         * Sets the pattern for log messages
         * @param pattern The pattern string (spdlog format)
         */
        static void SetPattern(const std::string &pattern);

    private:
        static std::shared_ptr<spdlog::logger> s_Logger;
    };

    /**
     * @brief Base class for loggable objects that automatically add class name and instance info
     */
    template <typename T = std::nullptr_t>
    class Loggable
    {
    public:
        explicit Loggable(std::string &&className) : m_ClassName(std::move(className)) {}

        template <typename = std::enable_if_t<!std::is_same_v<T, std::nullptr_t>>>
        Loggable() : Loggable(typeid(T).name())
        {
        }

        virtual ~Loggable() = default;

    protected:
        /**
         * Format a message with class name and instance information
         */
        template <typename... Args>
        std::string FormatMessage(fmt::format_string<Args...> fmt, Args &&...args) const
        {
            return fmt::format("[{}](0x{:X}) {}", m_ClassName, reinterpret_cast<uintptr_t>(this),
                               fmt::format(fmt, std::forward<Args>(args)...));
        }

        template <typename... Args>
        void WriteOut(fmt::format_string<Args...> fmt, Args &&...args) const
        {
            auto &loggerRef = Log::GetLogger();
            if (loggerRef)
            {
                loggerRef->info(FormatMessage(fmt, std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteInfo(fmt::format_string<Args...> fmt, Args &&...args) const
        {
            auto &loggerRef = Log::GetLogger();
            if (loggerRef)
            {
                loggerRef->info("INFO: {}", FormatMessage(fmt, std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteDebug(fmt::format_string<Args...> fmt, Args &&...args) const
        {
            auto &loggerRef = Log::GetLogger();
            if (loggerRef)
            {
                loggerRef->debug("DEBUG: {}", FormatMessage(fmt, std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteWarning(fmt::format_string<Args...> fmt, Args &&...args) const
        {
            auto &loggerRef = Log::GetLogger();
            if (loggerRef)
            {
                loggerRef->warn("WARN: {}", FormatMessage(fmt, std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteError(fmt::format_string<Args...> fmt, Args &&...args) const
        {
            auto &loggerRef = Log::GetLogger();
            if (loggerRef)
            {
                loggerRef->error("ERROR: {}", FormatMessage(fmt, std::forward<Args>(args)...));
            }
        }

        template <typename... Args>
        void WriteCritical(fmt::format_string<Args...> fmt, Args &&...args) const
        {
            auto &loggerRef = Log::GetLogger();
            if (loggerRef)
            {
                loggerRef->critical("CRITICAL: {}",
                                    FormatMessage(fmt, std::forward<Args>(args)...));
            }
        }

    private:
        std::string m_ClassName;
    };
} // namespace SF::Engine

// Convenience macros for logging
#define ENGINE_LOG_TRACE(...) ::SF::Engine::Log::GetLogger()->trace(__VA_ARGS__)
#define ENGINE_LOG_DEBUG(...) ::SF::Engine::Log::GetLogger()->debug(__VA_ARGS__)
#define ENGINE_LOG_INFO(...) ::SF::Engine::Log::GetLogger()->info(__VA_ARGS__)
#define ENGINE_LOG_WARN(...) ::SF::Engine::Log::GetLogger()->warn(__VA_ARGS__)
#define ENGINE_LOG_ERROR(...) ::SF::Engine::Log::GetLogger()->error(__VA_ARGS__)
#define ENGINE_LOG_CRITICAL(...) ::SF::Engine::Log::GetLogger()->critical(__VA_ARGS__)

#define ENGINE_LOG_ASSERT(expr, ...)                                  \
    do                                                                \
    {                                                                 \
        if (!(expr))                                                  \
        {                                                             \
            ENGINE_LOG_CRITICAL("Assertion failed: {}", __VA_ARGS__); \
            assert(false);                                            \
        }                                                             \
    } while (0)