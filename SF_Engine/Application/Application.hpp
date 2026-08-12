#pragma once

#include <stdarg.h>
#include <stddef.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <Filesystem/File.hpp>
#include <LowLevel/Rocket.hpp>

// Undefine problematic macros (microslop)
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

#if _PLATFORM_WINDOWS

// Must define NOMINMAX before windows.h to prevent min/max macro pollution
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define __INTRIN_H_ // Prevent intrinsics header inclusion
#include <windows.h>
#if GCC
#include <x86intrin.h>
#endif
#elif defined(_PLATFORM_MACOS)
// macOS
#include <mach-o/dyld.h>
#elif defined(_PLATFORM_LINUX)
// Linux
#include <unistd.h>
#endif

#include <Engine/VersionSemantic.hpp>
#include <UtilityClasses/NoCopy.hpp>
#include <Platform/PlatformIncludes.hpp>

#include <Scene/SceneManager.hpp>
#include <Rendering/Windows/WindowManager.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Engine/Log/Log.hpp>
#include <Engine/InitGame/GameInfo.hpp>
#include <TemplateLibrary/TypeTraits.hpp>
#include <Default/ImGuiDefaultWIDGETS.hpp>

#define INFO_CHECK \
    if (!Info)     \
    {              \
        return;    \
    }

namespace SF::Engine
{
    std::filesystem::path GetExecutablePathImpl();

    template <class ApplicationInfo>
    class Application : public virtual rocket::trackable, NoCopy
    {
        static_assert(std::is_base_of_v<GameInfo, ApplicationInfo>,
                      "ApplicationInfo must derive from GameInfo");
        friend class Engine;

    private:
        WindowManager *wndMgr = SF::Engine::WindowManager::Get();
        RenderSystem *renderer = SF::Engine::RenderSystem::Get();
        SceneManager *sceneMgr = SF::Engine::SceneManager::Get();
        std::unique_ptr<Engine> engine;

    public:
        enum InitializationReturn
        {
            Success,
            FailedToGetEngineModules,
            FailedToCreateGameWindow,
            NoApplicationlicationInfo,
            Failure
        };

        std::unique_ptr<ApplicationInfo> Info;
        Window *window;

        explicit Application(ApplicationInfo info, const Version &version = {1, 0, 0})
            : Info(std::make_unique<ApplicationInfo>(std::move(info)))
        {
            auto exeDir = GetExecutablePath().parent_path();
            std::filesystem::current_path(exeDir);
            engine = std::make_unique<Engine>(exeDir.string());

            wndMgr = SF::Engine::WindowManager::Get();
            renderer = SF::Engine::RenderSystem::Get();
            sceneMgr = SF::Engine::SceneManager::Get();
        }

        // call my init first :)
        virtual InitializationReturn Init()
        {
            if (!wndMgr || !renderer || !sceneMgr)
            {
                Log::Error("Failed to get engine modules\n");
                return InitializationReturn::FailedToGetEngineModules;
            }

            window = wndMgr->AddWindow();
            if (!window)
            {
                Log::Error("Failed to create window\n");
                return InitializationReturn::FailedToCreateGameWindow;
            }
            if (!Info)
            {
                Log::Error("ApplicationInfo not initialized\n");
                return InitializationReturn::NoApplicationlicationInfo;
            }
            window->SetTitle(static_cast<std::string>(Info->name));
            window->SetResizable(true);
            window->SetTitleColor(SF::Engine::Color());

            RegisterDefaultComponentWidgets();
            return InitializationReturn::Success;
        }

        // call my update first :)
        // Main loop  drive all module stages in the correct order:
        //   Normal  : SceneManager (Initialize / Start / Update+Render)
        //   Render  : RenderSystem (pipeline pass execution + present)
        virtual void Update()
        {
            wndMgr->Update();
            sceneMgr->Update();
            renderer->Update();
        }

        void AppLoop()
        {
            while (!window->IsClosed())
            {
                Update();
            }
            OnShutdown();
        }

    protected:
        virtual void OnShutdown() {} // optional override point, no-op by default

    public:
        /**
         * @brief Gets the Applicationlication's name.
         * @return The Applicationlication's name.
         */
        [[nodiscard]] const ::SFTL::String &GetName() const noexcept
        {
            INFO_CHECK
            return Info->name;
        }

        /**
         * @brief Sets the Applicationlication's name for driver support.
         * @param name The new Applicationlication name.
         */
        void SetName(::SFTL::String name)
        {
            INFO_CHECK
            Info->name = name;
        }

        /**
         * @brief Gets the Applicationlication's version.
         * @return The Applicationlication's version.
         */
        [[nodiscard]] const Version &GetVersion() const noexcept
        {
            INFO_CHECK
            return Info->version;
        }

        /**
         * @brief Sets the Applicationlication's version for driver support.
         * @param version The new Applicationlication version.
         */
        void SetVersion(const Version &version) noexcept
        {
            INFO_CHECK
            Info->version = version;
        }

        /**
         * @brief Checks if the Applicationlication has been started.
         * @return True if Start() has been called, false otherwise.
         */
        [[nodiscard]] bool IsStarted() const noexcept
        {
            return started_;
        }

        std::vector<File> GetAllModules() const
        {
            std::vector<File> result;

            const std::string exeDir = GetExecutablePath().parent_path().string();
            const char *pattern = "*.module";
            for (const auto &file : File::GetFiles(exeDir, pattern, false))
            {
                result.emplace_back(file);
            }

            return result;
        }

        std::filesystem::path GetExecutablePath() const
        {
            return GetExecutablePathImpl();
        }

    private:
        bool started_ = false;
        std::string path_;
        std::vector<File> modules_;

    public:
        enum class ShutdownReturn
        {
            Success,
            Failed
        };

        virtual void Shutdown()
        {
            OnShutdown();     // let derived game code clean up first, engine still alive
            engine.reset();   // single, ordered teardown call; same mechanism main.cpp relies on
            window = nullptr; // just clear the observer pointer, don't manually remove it
            started_ = false;
        }

        virtual ~Application()
        {
            if (started_)
                Shutdown();
        }
    };

}