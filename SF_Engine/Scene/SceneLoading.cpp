#include "SceneLoading.hpp"
#include <Camera/EditorCamera.hpp>
#include <Default/DefaultScene.hpp>
#include <Engine/Log/Log.hpp>
#include <Filesystem/File.hpp>
#include <XML/XMLReader.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

#ifdef _PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif
#include <Controllers/CameraController.hpp>

namespace SF::Engine
{

    // LoadedScene
    //
    // Concrete Scene subclass produced by all loaders. It owns the deserialized
    // scene data and exposes it through the standard Scene interface. Callers
    // can replace the camera or add systems after loading before handing it to
    // SceneManager::SetScene().

    class LoadedScene : public Scene
    {
    public:
        explicit LoadedScene(SceneRendererConfig cfg = {}, std::string name = "")
            : Scene(std::make_unique<CameraController>(), name, cfg)
        {
        }

        void Start() override {}
        bool IsPaused() const override { return false; }
    };

    // Internal helpers

    namespace
    {
        // Strip the "file://" prefix and normalise separators
        std::string FilePathFromURL(const std::string &url)
        {
            constexpr std::string_view prefix = "file://";
            std::string path = url.starts_with(prefix) ? url.substr(prefix.size()) : url;

#ifdef _PLATFORM_WINDOWS
            // "file:///C:/foo" → "C:/foo"
            if (path.starts_with("/") && path.size() > 2 && path[2] == ':')
                path = path.substr(1);
            // Forward-slashes are fine on modern Windows, but normalise anyway
            for (auto &c : path)
                if (c == '/')
                    c = '\\';
#endif
            return path;
        }

        // Deserialise XML text into a LoadedScene and return it.
        // Returns nullptr + fills error on failure.
        std::unique_ptr<Scene> ParseXML(const std::string &xmlText,
                                        std::string &outError)
        {
            XMLReader reader;
            if (!reader.LoadFromString(xmlText))
            {
                outError = "XML parse error: " + reader.GetLastError();
                return nullptr;
            }

            // Read optional renderer config from root attributes
            XMLNode root = reader.GetRootNode();
            SceneRendererConfig cfg;
            std::string name = root.GetAttribute("name");

            bool atmo = false, sun = false;
            root.GetAttribute("atmosphere", atmo);
            root.GetAttribute("sun", sun);
            cfg.enableAtmosphere = atmo;

            auto scene = std::make_unique<LoadedScene>(cfg, name);
            scene->Deserialize(root);
            return scene;
        }

#ifdef _PLATFORM_WINDOWS
        // Minimal WinINet HTTP GET → returns body as string, throws on failure
        std::string HttpGet(const std::string &urlStr)
        {
            HINTERNET hInternet = InternetOpenA(
                "SF_Engine/1.0",
                INTERNET_OPEN_TYPE_PRECONFIG,
                nullptr, nullptr, 0);
            if (!hInternet)
                throw std::runtime_error("InternetOpen failed: " +
                                         std::to_string(GetLastError()));

            HINTERNET hUrl = InternetOpenUrlA(
                hInternet, urlStr.c_str(), nullptr, 0,
                INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                    INTERNET_FLAG_SECURE, // ignored for http://, harmless for https://
                0);
            if (!hUrl)
            {
                InternetCloseHandle(hInternet);
                throw std::runtime_error("InternetOpenUrl failed: " +
                                         std::to_string(GetLastError()));
            }

            std::string body;
            char buf[4096];
            DWORD bytesRead = 0;
            while (InternetReadFile(hUrl, buf, sizeof(buf), &bytesRead) && bytesRead > 0)
                body.append(buf, bytesRead);

            InternetCloseHandle(hUrl);
            InternetCloseHandle(hInternet);
            return body;
        }
#else
        // Non-Windows stub  could be replaced with libcurl
        std::string HttpGet(const std::string &urlStr)
        {
            throw std::runtime_error("HTTP loading not implemented on this platform. "
                                     "URL: " +
                                     urlStr);
        }
#endif
    } // anonymous namespace

    // FileSceneSource

    SceneLoadResult FileSceneSource::Load(const std::string &url)
    {
        const std::string path = FilePathFromURL(url);

        if (!File::Exists(path))
            return {nullptr, "File not found: " + path, false};

        File f(path);
        if (!f.Open(FileMode::Read))
            return {nullptr, "Could not open file: " + path, false};

        const std::string xmlText = f.ReadAllText();
        f.Close();

        if (xmlText.empty())
            return {nullptr, "File is empty: " + path, false};

        std::string error;
        auto scene = ParseXML(xmlText, error);
        if (!scene)
            return {nullptr, error, false};

        Log::Info("SceneLoader: loaded '{}' from file", path);
        return {std::move(scene), {}, true};
    }

    SceneLoadResult FileSceneSource::Load(const URL &url)
    {
        // Reconstruct the raw path  authority is empty for file://, path is the rest
        std::string rawPath = url.path;
#ifdef _PLATFORM_WINDOWS
        if (rawPath.starts_with("/") && rawPath.size() > 2 && rawPath[2] == ':')
            rawPath = rawPath.substr(1);
        for (auto &c : rawPath)
            if (c == '/')
                c = '\\';
#endif
        return Load(rawPath);
    }

    static std::unordered_map<std::string, std::string> s_sceneMap; // name → xml text

    void MapSceneSource::Register(const std::string &name, const std::string &xmlText)
    {
        s_sceneMap[name] = xmlText;
    }

    SceneLoadResult MapSceneSource::Load(const std::string &name)
    {
        // Strip map:// prefix if present
        std::string key = name;
        constexpr std::string_view prefix = "map://";
        if (key.starts_with(prefix))
            key = key.substr(prefix.size());

        auto it = s_sceneMap.find(key);
        if (it == s_sceneMap.end())
            return {nullptr, "No scene registered with name: " + key, false};

        std::string error;
        auto scene = ParseXML(it->second, error);
        if (!scene)
            return {nullptr, error, false};

        Log::Info("SceneLoader: loaded '{}' from map", key);
        return {std::move(scene), {}, true};
    }

    SceneLoadResult MapSceneSource::Load(const URL &url)
    {
        // authority holds the name for map://sceneName
        const std::string key = url.authority.empty() ? url.path : url.authority;
        return Load(key);
    }

    SceneLoadResult HttpSceneSource::Load(const std::string &url)
    {
        std::string xmlText;
        try
        {
            xmlText = HttpGet(url);
        }
        catch (const std::exception &e)
        {
            return {nullptr, std::string("HTTP fetch failed: ") + e.what(), false};
        }

        if (xmlText.empty())
            return {nullptr, "HTTP response was empty for: " + url, false};

        std::string error;
        auto scene = ParseXML(xmlText, error);
        if (!scene)
            return {nullptr, error, false};

        Log::Info("SceneLoader: loaded scene from '{}'", url);
        return {std::move(scene), {}, true};
    }

    SceneLoadResult HttpSceneSource::Load(const URL &url)
    {
        return Load(url.ToString());
    }

} // namespace SF::Engine
