#pragma once
#include <Scene/Scene.hpp>

namespace SF::Engine
{
    struct PlayInEditorResult
    {
        bool success;
        std::string error;

    public:
        static PlayInEditorResult Success() { return {true, ""}; }
        static PlayInEditorResult Failure(std::string error) { return {false, std::move(error)}; }
        bool IsSuccess() const { return success; }

    private:
        PlayInEditorResult(bool success, std::string error)
            : success(success), error(std::move(error)) {}
    };

    class GameInstance
    {
    public:
        virtual void OnCreate() {}             // called once at engine startup
        virtual void OnDestroy() {}            // called at shutdown
        virtual void OnSceneLoad(Scene *) {}   // hook per load
        virtual void OnSceneUnload(Scene *) {} // hook per unload
    };
}