#pragma once

#include <Graphics/Windows/Window.hpp>
#include <Math/BasicMath.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <Camera/Camera.hpp>

namespace SF::Engine
{
    /**
     * @brief First-person fly camera driven from Window input.
     *
     * Controls (RMB look is NEVER blocked by ImGui window hover):
     *   Right-click + drag  : look (pitch / yaw)
     *   W / S               : forward / backward
     *   A / D               : strafe left / right
     *   Q / E               : down / up
     *   Shift               : 3x speed boost
     *   Scroll wheel        : FOV zoom
     *
     * Window::GetMousePositionDelta() returns RAW pixel delta (not dt-scaled).
     * lookSensitivity is therefore in degrees-per-pixel.
     */
    class EditorCamera : public Camera
    {
    public:
        float moveSpeed = 5.0f;
        float lookSensitivity = 0.12f; // degrees per raw pixel

        EditorCamera()
        {
            position = {0.0f, 0.0f, 0.0f};
            moveSpeed = 100.0f;
            UpdateVectors();
            SetInverseZ(true);
            SetFarPlaneInfinite(true); // oopsies
        }

        TypeId GetTypeId() const override
        {
            return TypeInformation<Component>::GetTypeId<EditorCamera>();
        }

        std::string_view GetTypeName() const override
        {
            return TypeInformation<Component>::GetTypeName<EditorCamera>();
        }

        void Update(Window *window, float dt, bool /*imguiWantsMouse*/, bool imguiWantsKeyboard) override
        {
            if (!window)
                return;

            bool rmb = window->GetMouseButton(MouseButton::Right) != InputAction::Release;

            //  Look: RMB drag
            if (rmb)
            {
                auto delta = window->GetMousePositionDelta();
                // GetMousePositionDelta() returns raw pixel delta this frame
                if (delta.x != 0.0 || delta.y != 0.0)
                {
                    yaw_ += static_cast<float>(delta.x) * lookSensitivity;
                    pitch_ -= static_cast<float>(delta.y) * lookSensitivity;
                    pitch_ = std::clamp(pitch_, -89.0f, 89.0f);
                    UpdateVectors();
                }
                if (!window->IsCursorHidden())
                    window->SetCursorHidden(true);
            }
            else
            {
                if (window->IsCursorHidden())
                    window->SetCursorHidden(false);
            }

            //  Move: always while RMB held, else respect keyboard focus
            bool canMove = rmb || !imguiWantsKeyboard;
            if (canMove)
            {
                float speed = moveSpeed * dt;

                // Shift boost : use the correct enum names from ButtonCodes.hpp
                bool shift =
                    window->GetKey(Key::ShiftLeft) != InputAction::Release ||
                    window->GetKey(Key::ShiftRight) != InputAction::Release;
                if (shift)
                    speed *= 3.0f;

                if (window->GetKey(Key::W) != InputAction::Release)
                    position += front_ * speed;
                if (window->GetKey(Key::S) != InputAction::Release)
                    position -= front_ * speed;
                if (window->GetKey(Key::A) != InputAction::Release)
                    position -= right_ * speed;
                if (window->GetKey(Key::D) != InputAction::Release)
                    position += right_ * speed;
                if (window->GetKey(Key::Q) != InputAction::Release)
                    position -= up_ * speed;
                if (window->GetKey(Key::E) != InputAction::Release)
                    position += up_ * speed;
            }

            //  FOV: scroll wheel
            // Always allow scroll when RMB is held (viewport zoom).
            // Otherwise only allow when ImGui isn't consuming the scroll.
            {
                float scroll = static_cast<float>(window->GetMouseScrollDelta().y);
                if (scroll != 0.0f)
                {
                    fovDeg -= scroll * 2.0f;
                    fovDeg = std::clamp(fovDeg, minFov, maxFov);
                }
            }
        }
    };
}
