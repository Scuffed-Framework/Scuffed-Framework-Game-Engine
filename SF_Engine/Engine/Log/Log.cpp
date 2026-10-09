#include "Log.hpp"
#include <iostream>

namespace SF::Engine
{
    std::shared_ptr<spdlog::logger> Log::s_Logger = nullptr;

    void Log::Init(const std::filesystem::path &filepath, const std::string &name)
    {
        try
        {
            // Console sink with colors
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_color_mode(spdlog::color_mode::always);       // CLion run console isn't a TTY
            consoleSink->set_color(spdlog::level::warn, "\033[38;5;208m"); // true orange (256-color)
            consoleSink->set_color(spdlog::level::err, consoleSink->red_bold);
            consoleSink->set_color(spdlog::level::critical, consoleSink->bold_on_red);
            consoleSink->set_pattern("%^[%T] [%l] %n: %v%$"); // %^..%$ colors the whole line

            // File sink
            auto parentPath = filepath.parent_path();
            if (!parentPath.empty())
            {
                std::filesystem::create_directories(parentPath);
            }

            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filepath.string(), true);
            fileSink->set_pattern("[%Y-%m-%d %T.%e] [%l] %n: %v");

            // Create logger. Do NOT call s_Logger->set_pattern() or spdlog::set_pattern()
            // afterwards: both overwrite the per-sink patterns above.
            s_Logger = std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{consoleSink, fileSink});
            s_Logger->set_level(spdlog::level::info);
            s_Logger->flush_on(spdlog::level::info);

            spdlog::register_logger(s_Logger);

            Info("Logging system initialized");
        } catch (const spdlog::spdlog_ex &ex)
        {
            std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        }
    }

    void Log::Shutdown()
    {
        if (s_Logger)
        {
            s_Logger->flush();
            s_Logger.reset();
        }
        spdlog::shutdown();
    }

    std::shared_ptr<spdlog::logger> &Log::GetLogger()
    {
        // Initialize with default if not already initialized
        if (!s_Logger)
        {
            static bool initialized = false;
            if (!initialized)
            {
                Init();
                initialized = true;
            }
        }
        return s_Logger;
    }

    void Log::SetLevel(spdlog::level::level_enum level)
    {
        if (s_Logger)
        {
            s_Logger->set_level(level);
            s_Logger->flush_on(level);
        }
    }

    void Log::SetPattern(const std::string &pattern)
    {
        if (s_Logger)
        {
            s_Logger->set_pattern(pattern);
        }
    }
} // namespace SF::Engine
