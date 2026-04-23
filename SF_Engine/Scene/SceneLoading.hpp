#include <memory>
#include <string>
#include "Scene.hpp"
#include <Networking/URL.hpp>

namespace SF::Engine
{
    struct SceneLoadResult
    {
        std::unique_ptr<Scene> scene;
        std::string error;
        bool success = false;
    };

    class SceneSource
    {
    public:
        virtual ~SceneSource() = default;
        virtual SceneLoadResult Load(const std::string &url) = 0;
        virtual SceneLoadResult Load(const URL &url) = 0;
    };

    class FileSceneSource : public SceneSource
    {
    public:
        SceneLoadResult Load(const std::string &url) override;
        SceneLoadResult Load(const URL &url) override;
    };

    class MapSceneSource : public SceneSource
    {
    public:
        /**
         * Register an in-memory XML scene under a name so it can be loaded
         * with map://sceneName.
         */
        static void Register(const std::string &name, const std::string &xmlText);

        SceneLoadResult Load(const std::string &url) override;
        SceneLoadResult Load(const URL &url) override;
    };

    class HttpSceneSource : public SceneSource
    {
    public:
        SceneLoadResult Load(const std::string &url) override;
        SceneLoadResult Load(const URL &url) override;
    };

    class SceneURLResolver
    {
    public:
        SceneLoadResult Load(const std::string &url)
        {
            if (url.starts_with("map://"))
                return mapSource.Load(url);
            if (url.starts_with("file://"))
                return fileSource.Load(url);
            if (url.starts_with("http://") || url.starts_with("https://"))
                return httpSource.Load(url);

            return {nullptr, "Unknown URL scheme", false};
        }
        SceneLoadResult Load(const URL &url)
        {
            if (url.IsMap())
                return mapSource.Load(url);
            if (url.IsFile())
                return fileSource.Load(url);
            if (url.IsRemote())
                return httpSource.Load(url);

            return {nullptr, "Unknown URL scheme: " + url.scheme, false};
        }

    private:
        MapSceneSource mapSource;
        FileSceneSource fileSource;
        HttpSceneSource httpSource;
    };
}