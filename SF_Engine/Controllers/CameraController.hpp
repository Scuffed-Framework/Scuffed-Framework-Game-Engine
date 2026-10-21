#pragma once
#include <Rendering/Camera/Camera.hpp>
#include <Rendering/Camera/EditorCamera.hpp>
#include "Controller.hpp"

namespace SF::Engine
{
    class CameraController : public StaticController<CameraController>
    {
    public:
        // Start possessed by the editor camera by default
        CameraController() : possessed_(&editorCamera_)
        {
        }

        void SetFrameInput(Window *w, bool imguiWantsMouse, bool imguiWantsKeyboard)
        {
            window = w;
            this->imguiWantsMouse = imguiWantsMouse;
            this->imguiWantsKeyboard = imguiWantsKeyboard;
        }

        // Possess any Camera, pass nullptr to fall back to editor cam
        void Possess(Camera *cam)
        {
            possessed_ = cam ? cam : &editorCamera_;
        }

        void ReleaseToEditor()
        {
            possessed_ = &editorCamera_;
        }

        // Drive the editor camera when it's active
        void Update(float dt) override
        {
            if (possessed_ == &editorCamera_)
                editorCamera_.Update(window, dt, imguiWantsMouse, imguiWantsKeyboard);
            // Game cameras update themselves via their own Update() / pawn tick
        }

        Camera *GetActive() { return possessed_; }
        EditorCamera &GetEditorCamera() { return editorCamera_; }

        bool IsEditorCameraActive() const { return possessed_ == &editorCamera_; }

    private:
        EditorCamera editorCamera_;
        Camera *possessed_; // non-owning  lifetime managed by scene/pawn
        Window *window;
        bool imguiWantsMouse;
        bool imguiWantsKeyboard;
    };
}