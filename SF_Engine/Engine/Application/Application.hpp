#pragma once

#ifdef _PLATFORM_LINUX
    #include <csignal>
#endif

#include <cstdarg>
#include <cstddef>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <3rdPartyLibs/Rocket.hpp>
#include <LowLevel/FileSystem/File.hpp>

// Undefine problematic macros (microslop)
#ifdef major
    #undef major
#endif
#ifdef minor
    #undef minor
#endif

#include <Engine/VersionSemantic.hpp>
#include <Platform/PlatformIncludes.hpp>
#include <UtilityClasses/NoCopy.hpp>

#include <Configuration/Default/ImGuiDefaultWidgets.hpp>
#include <Engine/InitGame/AppInfo.hpp>
#include <Engine/Log/Log.hpp>
#include <Platform/Windowing/WindowManager.hpp>
#include <Rendering/RHI/Viewport/Viewport.hpp>
#include <Rendering/RenderSystem.hpp>
#include <Scene/SceneManager.hpp>

#ifdef Started
    #undef Started
#endif
#ifdef Success
    #undef Success
#endif

namespace SF::Engine
{
    std::filesystem::path GetExecutablePathImpl();

    class Application : NoCopy
    {
        friend class Engine;

        WindowManager *wndMgr  = nullptr;
        RenderSystem *renderer = nullptr;
        SceneManager *sceneMgr = nullptr;
        std::unique_ptr<Engine> engine;
        bool started_ = false;
        std::string path_;
        std::vector<File> modules_;
        std::vector<std::unique_ptr<SceneViewport>> viewports;
        size_t reg{};

    public:
        enum InitializationReturn
        {
            Success,
            FailedToGetEngineModules,
            FailedToCreateGameWindow,
            NoApplicationInfo,
            Failure
        };

        enum class ShutdownReturn
        {
            Success,
            Failed
        };

        // Owned polymorphically; pass a derived AppInfo via std::make_unique<MyInfo>(...).
        std::unique_ptr<AppInfo> Info;

        Window *window{};

        explicit Application(std::unique_ptr<AppInfo> info);
        virtual ~Application();

        // call my init first :)
        virtual InitializationReturn Init();

        // call my update first :)
        // Main loop drives all module stages in the correct order:
        //   Normal  : SceneManager (Initialize / Start / Update+Render)
        //   Render  : RenderSystem (pipeline pass execution + present)
        virtual void Update();

        virtual void Shutdown();

        void AppLoop();

        /// Typed access to the info object for derived applications.
        template<class TInfo>
        [[nodiscard]] TInfo *InfoAs() const noexcept
        {
            static_assert(std::is_base_of_v<AppInfo, TInfo>, "TInfo must derive from AppInfo");
            return static_cast<TInfo *>(Info.get());
        }

        /**
         * @brief Gets the Application's name.
         */
        [[nodiscard]] const std::string &GetName() const noexcept;

        /**
         * @brief Sets the Application's name for driver support.
         */
        void SetName(const std::string &name) const;

        /**
         * @brief Gets the Application's version.
         */
        [[nodiscard]] const Version &GetVersion() const noexcept;

        /**
         * @brief Sets the Application's version for driver support.
         */
        void SetVersion(const Version &version) const noexcept;

        /**
         * @brief Checks if the Application has been started.
         * @return True if Start() has been called, false otherwise.
         */
        [[nodiscard]] bool IsStarted() const noexcept { return started_; }

        static std::vector<File> GetAllModules();
        static std::filesystem::path GetExecutablePath() { return GetExecutablePathImpl(); }

    protected:
        virtual void OnShutdown() {} // optional override point, no-op by default
    };

} // namespace SF::Engine
