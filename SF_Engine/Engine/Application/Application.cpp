#include "Application.hpp"

#include <cassert>

#if _PLATFORM_WINDOWS
    // Must define NOMINMAX before windows.h to prevent min/max macro pollution
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #define __INTRIN_H_ // Prevent intrinsics header inclusion
    #include <windows.h>
#elif defined(_PLATFORM_MACOS)
    #include <mach-o/dyld.h>
#elif defined(_PLATFORM_LINUX)
    #include <unistd.h>
    #include <x86intrin.h>
#endif

namespace SF::Engine
{
    std::filesystem::path GetExecutablePathImpl()
    {
#if _PLATFORM_WINDOWS
        std::vector<wchar_t> buffer(MAX_PATH);

        DWORD size = 0;
        while (true)
        {
            size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

            if (size == 0)
                return {};

            if (size < buffer.size())
                break;

            buffer.resize(buffer.size() * 2);
        }

        return std::filesystem::path(buffer.data());

#elif _PLATFORM_MACOS
        uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);

        std::vector<char> buffer(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            return {};

        return std::filesystem::weakly_canonical(buffer.data());

#elif _PLATFORM_LINUX
        std::vector<char> buffer(1024);
        ssize_t size = 0;

        while (true)
        {
            size = readlink("/proc/self/exe", buffer.data(), buffer.size());
            if (size < 0)
                return {};

            if (size < static_cast<ssize_t>(buffer.size()))
                break;

            buffer.resize(buffer.size() * 2);
        }

        return {std::string(buffer.data(), size)};

#else
        return {};
#endif
    }

    Application::Application(std::unique_ptr<AppInfo> info) : Info(std::move(info))
    {
        auto exeDir = GetExecutablePath().parent_path();
        std::filesystem::current_path(exeDir);
        engine = std::make_unique<Engine>(exeDir.string());
        engine->SetGameInfo(Info.get());

        wndMgr   = WindowManager::Get();
        renderer = RenderSystem::Get();
        sceneMgr = SceneManager::Get();

        viewports.push_back(std::make_unique<SceneViewport>(UVec2{800, 600}));

        renderer->OnRecordViewports().Add(
                [this](VkCommandBuffer cmd, std::size_t frameIndex)
                {
                    for (auto &vp: viewports)
                    {
                        vp->Tick(frameIndex);
                        vp->PrepareForRender(cmd);
                        vp->BeginRendering(cmd);
                        // scene draws for this viewport go here

                        vp->EndRendering(cmd);
                        vp->PrepareForSample(cmd);
                    }
                });

        // Capture the raw viewport pointer, not a reference into the vector (it dangles if viewports grows).
        for (auto &vp: viewports)
            reg = UIRegistry::Get().Register([p = vp.get()] { DrawViewport(p); });
    }

    Application::InitializationReturn Application::Init()
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
            return InitializationReturn::NoApplicationInfo;
        }
        window->SetTitle(static_cast<std::string>(Info->name));
        window->SetResizable(true);
        window->SetTitleColor(SF::Engine::Color());

        RegisterDefaultComponentWidgets();
        return InitializationReturn::Success;
    }

    void Application::Update()
    {
        wndMgr->Update();
        sceneMgr->Update();
        renderer->Update();
    }

    void Application::AppLoop()
    {
        while (!window->IsClosed())
        {
            Update();
        }
        // Drain the GPU BEFORE anything is torn down. The loop exits with up to a few frames
        // still in flight, and OnShutdown()/member destructors (viewport images, editor
        // panels' textures, ...) run before the engine's own idle-wait; destroying an image a
        // pending command buffer still references (VUID-vkDestroyImage-image-01000) is what
        // lost the device at shutdown.
        if (renderer)
            RenderSystem::CheckVkResult(vkDeviceWaitIdle(*renderer->GetLogicalDevice()));
        OnShutdown();
    }

    const std::string &Application::GetName() const noexcept
    {
        assert(Info);
        return Info->name;
    }

    void Application::SetName(const std::string &name) const
    {
        assert(Info);
        Info->name = name;
    }

    const Version &Application::GetVersion() const noexcept
    {
        assert(Info);
        return Info->version;
    }

    void Application::SetVersion(const Version &version) const noexcept
    {
        assert(Info);
        Info->version = version;
    }

    std::vector<File> Application::GetAllModules()
    {
        std::vector<File> result;

        const std::string exeDir = GetExecutablePath().parent_path().string();
        const char *pattern      = "*.module";
        for (const auto &file: File::GetFiles(exeDir, pattern, false))
        {
            result.emplace_back(file);
        }

        return result;
    }

    void Application::Shutdown()
    {
        OnShutdown();       // let derived game code clean up first, engine still alive
        engine.reset();     // single, ordered teardown call; same mechanism main.cpp relies on
        window   = nullptr; // just clear the observer pointer, don't manually remove it
        started_ = false;
    }

    Application::~Application()
    {
        // Derived-class members are already gone by now; this covers any path that skipped
        // AppLoop() and protects the members destroyed after this body (viewports, ...), which
        // are declared after `engine` and so die BEFORE it (and before RenderSystem's own wait).
        if (renderer && renderer->GetLogicalDevice())
            vkDeviceWaitIdle(*renderer->GetLogicalDevice());
        if (started_)
            Application::Shutdown();
    }
} // namespace SF::Engine
