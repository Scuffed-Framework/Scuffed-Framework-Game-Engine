#pragma once

// Macro Section
#define MEMORY_BARRIER() asm volatile("" ::: "memory")
#define PREFETCH(addr) asm volatile("prefetcht0 %0" ::"m"(*(const char *)(addr)))
#define PREFETCHW(addr) asm volatile("prefetcht0 %0" ::"m"(*(char *)(addr)))
#define PAUSE() asm volatile("pause")
#define CPU_RELAX() asm volatile("rep nop" ::: "memory")
#define NO_MANGLE __attribute__((visibility("default"))) extern "C"
#define NO_MANGLE_EXPORT __attribute__((visibility("default"))) __declspec(dllexport) extern "C"

#include <LayerStack/LayerStack.hpp>
#include "Log/Log.hpp"
#include "Module.hpp"

#include <Application/App.hpp>
#include <LowLevel/Rocket.hpp>
#include <Math/Time/Time.hpp>
#include <cstdint>
#include <string>
#include <string_view>
#include <map>
#include <Networking/URL.hpp>
#include <Scene/Scene.hpp>
#include <Scene/SceneLoading.hpp>

#include <UtilityClasses/Formatter.hpp>
#include <Threading/ThreadPool.hpp>

#include "InitGame/GameInstance.hpp"

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

// Because windows.h is a stinking pile of garbage
#ifdef MAJOR
#undef MAJOR
#endif
#ifdef MINOR
#undef MINOR
#endif
// also might work for linux/mac stuff but idk

namespace SF::Engine
{
    // Provide a global constant version of the engine
    inline constexpr Version EngineVersion{};

    class Engine : NoCopy
    {
    public:
        /**
         * Gets the engines instance.
         * @return The current engine instance.
         */
        static Engine *Get()
        {
            return Instance;
        }

        /**
         * Carries out the setup for basic engine components and the engine. Call {@link Engine#Run}
         * after creating a instance.
         * @param argv0 The first argument passed to main.
         * @param moduleFilter A filter for blacklisting/whitelisting modules.
         * @param startUpURL URL of a local map scene, or a server that will send one.
         */
        explicit Engine(std::string argv0, ModuleFilter &&moduleFilter = {}, URL startUpURL = URL("map://startup")); // TODO: Make startup look through scene metadata to find "<StartUpScene>true</StartupScene>"

        ~Engine();

        /**
         * The update function for the updater.
         * @return {@code EXIT_SUCCESS} or {@code EXIT_FAILURE}
         */
        int32_t Run();

        /**
         * Gets the first argument passed to main.
         * @return The first argument passed to main.
         */
        const std::string &GetArgv0() const
        {
            return argv0;
        };

        /**
         * Gets the engine's version.
         * @return The engine's version.
         */
        const Version &GetVersion() const
        {
            return version;
        }

        /**
         * Gets the current application.
         * @return The renderer manager.
         */
        App *GetApp() const
        {
            return app.get();
        }

        /**
         * Sets the current application to a new application.
         * @param app The new application.
         */
        void SetApp(std::unique_ptr<App> &&app)
        {
            this->app = std::move(app);
        }

        /**
         * Gets the fps limit.
         * @return The frame per second limit.
         */
        float GetFpsLimit() const
        {
            return fpsLimit;
        }

        /**
         * Sets the fps limit. -1 disables limits.
         * @param fpsLimit The new frame per second limit.
         */
        void SetFpsLimit(float fpsLimit)
        {
            this->fpsLimit = fpsLimit;
        }

        /**
         * Gets if the engine is running.
         * @return If the engine is running.
         */
        bool IsRunning() const
        {
            return running;
        }

        /**
         * Gets the delta (seconds) between updates.
         * @return The delta between updates.
         */
        const ApplicationTime &GetDelta() const
        {
            return deltaUpdate.change;
        }

        /**
         * Gets the delta (seconds) between renders.
         * @return The delta between renders.
         */
        const ApplicationTime &GetDeltaRender() const
        {
            return deltaRender.change;
        }

    private:
        UpdateClock<> ups, fps;

    public:
        /**
         * Gets the average UPS over a short interval.
         * @return The updates per second.
         */
        uint32_t GetUps() const
        {
            return ups.value_;
        }

        /**
         * Gets the average FPS over a short interval.
         * @return The frames per second.
         */
        uint32_t GetFps() const
        {
            return fps.value_;
        }

        /**
         * Requests the engine to stop the game-loop.
         */
        void RequestClose()
        {
            running = false;
        }

        /**
         * Gets the layer stack.
         * @return Reference to the layer stack.
         */
        LayerStack &GetLayerStack()
        {
            return layerStack;
        }

        /**
         * Gets the layer stack (const version).
         * @return Const reference to the layer stack.
         */
        const LayerStack &GetLayerStack() const
        {
            return layerStack;
        }

        /**
         * Push a layer onto the layer stack.
         * @param layer The layer to push.
         */
        void PushLayer(std::shared_ptr<Layer> layer)
        {
            layerStack.PushLayer(std::move(layer));
        }

        /**
         * Push an overlay onto the layer stack.
         * @param overlay The overlay to push.
         */
        void PushOverlay(std::shared_ptr<Layer> overlay)
        {
            layerStack.PushOverlay(std::move(overlay));
        }

        /**
         * Pop a layer from the layer stack.
         * @param layer The layer to pop.
         */
        void PopLayer(const std::shared_ptr<Layer> &layer)
        {
            layerStack.PopLayer(layer);
        }

        /**
         * Pop an overlay from the layer stack.
         * @param overlay The overlay to pop.
         */
        void PopOverlay(const std::shared_ptr<Layer> &overlay)
        {
            layerStack.PopOverlay(overlay);
        }

        /**
         * Dispatch an event to the layer stack.
         * @param event The event to dispatch.
         */
        template <typename EventType>
        void DispatchEventToLayers(EventType &event)
        {
            layerStack.DispatchEvent(event);
        }

    private:
        void CreateModule(Module::RegistryMap::const_iterator it, const ModuleFilter &filter);
        void DestroyModule(TypeId id);
        void UpdateStage(Module::Stage stage);

        static Engine *Instance;

        std::string argv0;
        Version version;

        std::unique_ptr<App> app;

        std::map<TypeId, std::unique_ptr<Module>> modules;
        std::map<Module::Stage, std::vector<TypeId>> moduleStages;

        LayerStack layerStack;

        float fpsLimit;
        bool running;

        DeltaTime deltaUpdate, deltaRender;
        ElapsedTime elapsedUpdate, elapsedRender;

        SceneURLResolver resolver;

    public:
        const std::thread::id g_main_thread_id = std::this_thread::get_id();
        const std::thread::id g_render_thread_id = std::thread::id(); // Set to main thread by default, will be updated if a separate render thread is used

        const std::thread::id &GetMainThreadId() const
        {
            return g_main_thread_id;
        }

        const std::thread::id &GetRenderThreadId() const
        {
            return g_render_thread_id;
        }

        bool IsMainThread() const
        {
            return std::this_thread::get_id() == g_main_thread_id;
        }

    private:
        ThreadPool threadPool{std::max(1u, std::thread::hardware_concurrency() - 1)}; // Leave one thread for the main loop

    public:
        ThreadPool &GetThreadPool()
        {
            return threadPool;
        }

    protected:
        std::unique_ptr<GameInstance> gameInstance;

    public:
        GameInstance *GetGameInstance() const
        {
            return gameInstance.get();
        }
        static XMLReader writer;
    };

    inline std::filesystem::path GetEngineRootPath()
    {
        // Get the path of the executable
        std::filesystem::path exePath = std::filesystem::current_path();
        return exePath;
    }

    inline std::filesystem::path GetEngineAssetsPath()
    {
        return GetEngineRootPath() / "Assets"; // todo: build into .rsc
    }
}