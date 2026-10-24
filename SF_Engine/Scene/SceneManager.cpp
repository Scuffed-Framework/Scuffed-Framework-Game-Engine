#include "SceneManager.hpp"
#include <Configuration/Default/DefaultScene.hpp>

#include <algorithm>
#include <cctype>

namespace SF::Engine
{
    namespace fs = std::filesystem;
    namespace
    {
        std::function<void()> gOnReplaced;

        fs::path Norm(const fs::path &p) { return fs::absolute(p).lexically_normal(); }

        // fs::equivalent handles case-insensitive filesystems, symlinks, etc. but needs both paths to
        // exist; fall back to a lexical comparison otherwise (e.g. the old path after a move).
        bool SamePath(const fs::path &a, const fs::path &b)
        {
            std::error_code ec;
            if (fs::equivalent(a, b, ec) && !ec)
                return true;
            return Norm(a) == Norm(b);
        }

        bool HasSceneExtension(const fs::path &p)
        {
            std::string ext = p.extension().string();
            ranges::transform(ext, ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return ext == std::string(SceneFileExtension);
        }

        void FireReplaced()
        {
            if (gOnReplaced)
                gOnReplaced();
        }
    } // namespace


    SceneManager::SceneManager()
    {
        // Intentionally empty.
        // Engine::Engine() is responsible for constructing the startup scene
        // (DefaultScene or a loaded one) and handing it to us via SetScene().
        // Creating a DefaultScene here causes a double-construction:
        //   1. SceneManager() makes one scene, stores it in `scene`
        //   2. Engine::Engine() makes another and calls SetScene(), which
        //      move-assigns `scene`, destroying the first one mid-construction.
        // That destruction fires ~Scene() -> ~vector<SceneLight> -> ~SceneLight
        // -> vtable dispatch into garbage -> 0xC0000005.
    }

    void SceneManager::ApplyPendingOpen()
    {
        if (!pendingOpen || !scene)
        {
            pendingOpen.reset();
            return;
        }

        const fs::path file = std::move(*pendingOpen);
        pendingOpen.reset();

        // TODO: make sure the GPU is done with the old entities' meshes/buffers before they are
        // destroyed (RenderSystem wait-idle, or deferred resource destruction).

        if (!scene->ReadXML(file.string()))
            return; // unreadable or newer-version file: scene left as it was

        // Systems may hold Entity*/component pointers into the old entity set. Drop them and let the
        // normal Start() path rebuild them this frame, before anything updates.
        scene->ClearSystems();
        scene->started_ = false;

        scene->SetPath(file);
        FireReplaced();
    }

    void SceneManager::Update()
    {
        if (pendingScene)
        {
            if (scene)
                scene->Stop();

            scene = std::move(pendingScene);

            if (auto *gi = Engine::Get()->GetGameInstance())
                gi->OnSceneLoad(scene.get());

            FireReplaced(); // selections etc. holding Entity* from the old scene are now stale
        }

        if (!scene)
            return;

        // Initialize deferred GPU resources (renderer, meshes, LightManager)
        // on the first Update() after all modules including RenderSystem are ready.
        if (!scene->initialized_)
            scene->Initialize();

        // Scene file loads run here: between frames, before systems update or anything renders.
        ApplyPendingOpen();

        if (!scene->started_)
        {
            scene->Start();
            scene->started_ = true;
        }

        scene->Update();
        scene->Render();
    }

    const fs::path &CurrentScenePath()
    {
        static const fs::path kEmpty;
        const Scene *s = SceneManager::Get()->GetScene();
        return s ? s->GetPath() : kEmpty;
    }

    bool CurrentScenePathNotEmpty() { return !CurrentScenePath().empty(); }

    bool IsFileCurrentCurrentScene(const fs::path &file)
    {
        const auto &cur = CurrentScenePath();
        return !cur.empty() && SamePath(cur, file);
    }

    bool OpenScene(const fs::path &file)
    {
        Scene *scene = SceneManager::Get()->GetScene();
        std::error_code ec;
        if (!scene || !fs::is_regular_file(file, ec))
            return false;

        SceneManager::Get()->RequestOpen(file);
        return true;
    }

    bool SaveScene()
    {
        Scene *scene = SceneManager::Get()->GetScene();
        if (!scene || scene->GetPath().empty())
            return false;
        return scene->SaveXML(scene->GetPath().string());
    }

    bool SaveSceneAs(const fs::path &fileIn)
    {
        Scene *scene = SceneManager::Get()->GetScene();
        if (!scene)
            return false;

        fs::path file = fileIn;
        if (!HasSceneExtension(file))
            file += std::string(SceneFileExtension);

        std::error_code ec;
        if (file.has_parent_path())
            fs::create_directories(file.parent_path(), ec);

        if (!scene->SaveXML(file.string()))
            return false;

        scene->SetPath(file);
        return true;
    }

    fs::path CreateDefaultScene(const fs::path &dir, const std::string &name)
    {
        const std::string ext(SceneFileExtension);
        std::error_code ec;
        fs::create_directories(dir, ec);

        fs::path file = dir / (name + ext);
        for (int i = 1; fs::exists(file, ec); ++i)
            file = dir / (name + " (" + std::to_string(i) + ")" += ext);

        TemplateScene tmp(name);
        return tmp.SaveXML(file.string()) ? file : fs::path{};
    }

    void NotifyMovedScene(const fs::path &from, const fs::path &to)
    {
        Scene *scene = SceneManager::Get()->GetScene();
        if (scene && !scene->GetPath().empty() && SamePath(scene->GetPath(), from))
            scene->SetPath(to);
    }

    void NotifyDeletedScene(const fs::path &file)
    {
        // The scene stays loaded in memory, but Ctrl+S will now ask where to save.
        Scene *scene = SceneManager::Get()->GetScene();
        if (scene && !scene->GetPath().empty() && SamePath(scene->GetPath(), file))
            scene->ClearPath();
    }

    void SetOnSceneReplaced(std::function<void()> cb) { gOnReplaced = std::move(cb); }
} // namespace SF::Engine
