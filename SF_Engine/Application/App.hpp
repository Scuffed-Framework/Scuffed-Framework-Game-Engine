#pragma once

#include <stdarg.h>
#include <stddef.h>

// Include standard library headers in dependency order
#include <cstdint>
#include <filesystem>
#include <memory>  // For std::allocator
#include <string>
#include <string_view>
#include <vector>

#include <Files/File.hpp>
#include <LowLevel/Rocket.hpp>

// Undefine problematic macros (fuck you microsoft)
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

// Platform-specific includes come AFTER standard library
#if _PLATFORM_WINDOWS

// Must define NOMINMAX before windows.h to prevent min/max macro pollution
#ifndef NOMINMAX  // Fuck you again microsoft
#define NOMINMAX
#endif

#define __INTRIN_H_  // Prevent intrinsics header inclusion
#include <windows.h>
#if GCC
#include <x86intrin.h>
#endif
#elif defined(__APPLE__)
// macOS
#include <mach-o/dyld.h>
#elif defined(__linux__)
// Linux
#include <unistd.h>
#endif

#include <Engine/VersionSemantic.hpp>

namespace SF::Engine
{
    /**
     * @brief Base class representing an application with lifecycle management.
     *
     * Applications can be started, updated, and switched between. Each app
     * has a name and version for identification and driver support.
     */

    class App : public virtual rocket::trackable
    {
        friend class Engine;

    public:
        explicit App(std::string name, const Version& version = {1, 0, 0})
            : name_(std::move(name)), version_(version)
        {
        }

        virtual ~App() = default;

        // Prevent copying, allow moving
        App(const App&) = delete;
        App& operator=(const App&) = delete;
        App(App&&) noexcept = default;
        App& operator=(App&&) noexcept = default;

        /**
         * @brief Called when switching to this app from another.
         *
         * Use this method to initialize resources and prepare the application
         * for active use. Will only be called once per activation.
         */
        virtual void Start() = 0;

        /**
         * @brief Called each frame before the module update pass.
         *
         * Implement application-specific logic and state updates here.
         * This method is called continuously while the app is active.
         */
        virtual void Update() = 0;

        /**
         * @brief Gets the application's name.
         * @return The application's name.
         */
        [[nodiscard]] const std::string& GetName() const noexcept
        {
            return name_;
        }

        /**
         * @brief Sets the application's name for driver support.
         * @param name The new application name.
         */
        void SetName(std::string_view name)
        {
            name_ = name;
        }

        /**
         * @brief Gets the application's version.
         * @return The application's version.
         */
        [[nodiscard]] const Version& GetVersion() const noexcept
        {
            return version_;
        }

        /**
         * @brief Sets the application's version for driver support.
         * @param version The new application version.
         */
        void SetVersion(const Version& version) noexcept
        {
            version_ = version;
        }

        /**
         * @brief Checks if the application has been started.
         * @return True if Start() has been called, false otherwise.
         */
        [[nodiscard]] bool IsStarted() const noexcept
        {
            return started_;
        }

        std::filesystem::path GetExecutablePath() const;

        std::vector<File> GetAllModules() const
        {
            std::vector<File> result;

            const std::string exeDir = GetExecutablePath().parent_path().string();
            const char* pattern = "*.module";
            for (const auto& file : File::GetFiles(exeDir, pattern, false))
            {
                result.emplace_back(file);
            }

            return result;
        }

    private:
        bool started_ = false;
        std::string name_;
        Version version_;
        std::string path_;
        std::vector<File> modules_;
    };
}
/*
App Dir example:
image: https://i.imgur.com/1n0bX4l.png

*/