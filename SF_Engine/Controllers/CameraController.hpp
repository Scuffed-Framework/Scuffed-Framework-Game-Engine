#pragma once
#include <Camera/Camera.hpp>
#include <Camera/EditorCamera.hpp>

namespace SF::Engine
{
    class CameraController
    {
    public:
        // Start possessed by the editor camera by default
        CameraController() : possessed_(&editorCamera_)
        {
        }

        // Possess any ACamera, pass nullptr to fall back to editor cam
        void Possess(ACamera *cam)
        {
            possessed_ = cam ? cam : &editorCamera_;
        }

        void ReleaseToEditor()
        {
            possessed_ = &editorCamera_;
        }

        // Drive the editor camera when it's active
        void Update(Window *window, float dt,
                    bool imguiWantsMouse, bool imguiWantsKeyboard)
        {
            if (possessed_ == &editorCamera_)
                editorCamera_.Update(window, dt, imguiWantsMouse, imguiWantsKeyboard);
            // Game cameras update themselves via their own Update() / pawn tick
        }

        ACamera *GetActive() { return possessed_; }
        EditorCamera &GetEditorCamera() { return editorCamera_; }

        bool IsEditorCameraActive() const { return possessed_ == &editorCamera_; }

    private:
        EditorCamera editorCamera_;
        ACamera *possessed_; // non-owning  lifetime managed by scene/pawn
    };
}