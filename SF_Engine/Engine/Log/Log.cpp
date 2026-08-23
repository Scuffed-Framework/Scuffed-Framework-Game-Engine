#include "Log.hpp"
#include <iostream>

namespace SF::Engine
{
    std::shared_ptr<spdlog::logger> Log::s_Logger = nullptr;

    void Log::Init(const std::filesystem::path& filepath, const std::string& name)
    {
        try
        {
            // Create sinks
            std::vector<spdlog::sink_ptr> sinks;

            // Console sink with colors
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_pattern("%^[%T] %n: %v%$");
            consoleSink->set_color(spdlog::level::warn, consoleSink->yellow);
            consoleSink->set_color(spdlog::level::err,  consoleSink->red_bold);
            consoleSink->set_color(spdlog::level::critical, consoleSink->bold_on_red);
            
            // File sink
            auto parentPath = filepath.parent_path();
            if (!parentPath.empty())
            {
                std::filesystem::create_directories(parentPath);
            }

            auto fileSink =
                std::make_shared<spdlog::sinks::basic_file_sink_mt>(filepath.string(), true);
            fileSink->set_pattern("[%Y-%m-%d %T.%e] [%l] %n: %v");

            sinks.push_back(consoleSink);
            sinks.push_back(fileSink);

            // Create logger
            s_Logger = std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{consoleSink, fileSink});
            s_Logger->set_pattern("%^[%H:%M:%S] [%l] %v%$");   // %^...%$ colors the WHOLE line, not just %l

            s_Logger->set_level(spdlog::level::info);
            s_Logger->flush_on(spdlog::level::info);

            // Register the logger
            spdlog::register_logger(s_Logger);

            // Set default pattern if not already set by sinks
            spdlog::set_pattern("[%T] [%l] %n: %v");

            Info("Logging system initialized");
        }
        catch (const spdlog::spdlog_ex& ex)
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

    std::shared_ptr<spdlog::logger>& Log::GetLogger()
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

    void Log::SetPattern(const std::string& pattern)
    {
        if (s_Logger)
        {
            s_Logger->set_pattern(pattern);
        }
    }
}  // namespace SF::Engine