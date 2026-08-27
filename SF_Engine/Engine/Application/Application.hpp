#pragma once

#include <stdarg.h>
#include <stddef.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <LowLevel/FileSystem/File.hpp>
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
#elif defined(_PLATFORM_MACOS)
// macOS
#include <mach-o/dyld.h>
#elif defined(_PLATFORM_LINUX)
// Linux
#include <unistd.h>
#include <x86intrin.h>
#endif

#include <Engine/VersionSemantic.hpp>
#include <UtilityClasses/NoCopy.hpp>
#include <Platform/PlatformIncludes.hpp>

#include <Scene/SceneManager.hpp>
#include <Platform/Windows/WindowManager.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Engine/Log/Log.hpp>
#include <Engine/InitGame/GameInfo.hpp>
#include <TemplateLibrary/TypeTraits.hpp>
#include <Configuration/Default/ImGuiDefaultWIDGETS.hpp>
#include <Rendering/Viewport/Viewport.hpp>
#include <Rendering/RenderSystem.hpp>

#ifdef Started
#undef Started
#endif
#ifdef Success
#undef Success
#endif

namespace SF::Engine
{
    std::filesystem::path GetExecutablePathImpl();

    template <class ApplicationInfo>
    class Application : NoCopy
    {
        static_assert(std::is_base_of_v<GameInfo, ApplicationInfo>,
                      "ApplicationInfo must derive from GameInfo");
        friend class Engine;

    private:
        WindowManager *wndMgr = WindowManager::Get();
        RenderSystem *renderer = RenderSystem::Get();
        SceneManager *sceneMgr = SceneManager::Get();
        std::unique_ptr<Engine> engine;
        bool started_ = false;
        std::string path_;
        std::vector<File> modules_;
        std::vector<std::unique_ptr<SceneViewport>> viewports;
        size_t reg;

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

        auto InfoCheck() -> void
        {
            if(!Info) return;
        }

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

            viewports.push_back(std::make_unique<SceneViewport>(UVec2{800, 600}));

            renderer->OnRecordViewports().Add([this](VkCommandBuffer cmd, std::size_t frameIndex)
            {
                for (auto &vp : viewports)
                {
                    vp->Tick(frameIndex);
                    vp->PrepareForRender(cmd);
                    vp->BeginRendering(cmd);
                    // scene draws for this viewport go here

                    vp->EndRendering(cmd);
                    vp->PrepareForSample(cmd);
                }
            });
            for (auto &vp : viewports)
                 reg = UIRegistry::Get().Register([this, &vp]{DrawViewport(vp.get());});;
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

    protected:
        virtual void OnShutdown() {} // optional override point, no-op by default

    public:
        void AppLoop()
        {
            while (!window->IsClosed())
            {
                Update();
            }
            OnShutdown();
        }

        /**
         * @brief Gets the Applicationlication's name.
         * @return The Applicationlication's name.
         */
        [[nodiscard]] const std::string &GetName() const noexcept
        {
            InfoCheck();
            return Info->name;
        }

        /**
         * @brief Sets the Applicationlication's name for driver support.
         * @param name The new Applicationlication name.
         */
        void SetName(std::string name)
        {
            InfoCheck();
            Info->name = name;
        }

        /**
         * @brief Gets the Applicationlication's version.
         * @return The Applicationlication's version.
         */
        [[nodiscard]] const Version &GetVersion() const noexcept
        {
            InfoCheck();
            return Info->version;
        }

        /**
         * @brief Sets the Applicationlication's version for driver support.
         * @param version The new Applicationlication version.
         */
        void SetVersion(const Version &version) noexcept
        {
            InfoCheck();
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