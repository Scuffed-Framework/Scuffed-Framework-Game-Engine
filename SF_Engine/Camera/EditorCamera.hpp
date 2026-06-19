#pragma once

#include <Graphics/Windows/Window.hpp>
#include <glm/glm.hpp>
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
    class EditorCamera : public ACamera
    {
    public:
        float moveSpeed = 5.0f;
        float lookSensitivity = 0.12f; // degrees per raw pixel
        float fovDeg = 60.0f;
        float nearPlane = 0.05f;
        float farPlane = 200000.0f;
        float minFov = 10.0f;
        float maxFov = 120.0f;

        float GetFieldOfView() const override
        {
            return fovDeg;
        }

        EditorCamera()
        {
            position_ = {0.0f, 0.0f, 0.0f};
            moveSpeed = 100.0f;
            UpdateVectors();
        }

        // imguiWantsKeyboard : true when an ImGui text field has focus
        // imguiWantsMouse    : true when the mouse is over any ImGui window
        //
        // RMB look intentionally ignores imguiWantsMouse: ImGui sets that flag
        // whenever ANY window is hovered (including transparent overlays), which
        // would make right-click look permanently broken in an editor layout.
        void Update(Window *window, float dt,
                    bool /*imguiWantsMouse*/, bool imguiWantsKeyboard)
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
                    position_ += front_ * speed;
                if (window->GetKey(Key::S) != InputAction::Release)
                    position_ -= front_ * speed;
                if (window->GetKey(Key::A) != InputAction::Release)
                    position_ -= right_ * speed;
                if (window->GetKey(Key::D) != InputAction::Release)
                    position_ += right_ * speed;
                if (window->GetKey(Key::Q) != InputAction::Release)
                    position_ -= up_ * speed;
                if (window->GetKey(Key::E) != InputAction::Release)
                    position_ += up_ * speed;
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

        glm::mat4 GetView() const
        {
            return glm::lookAt(position_, position_ + front_, glm::vec3(0, 1, 0));
        }

        glm::mat4 GetProjection(float aspect) const
        {
            glm::mat4 p = glm::perspective(
                glm::radians(fovDeg), aspect, nearPlane, farPlane);
            p[1][1] *= -1.0f; // Vulkan Y-flip
            return p;
        }

        glm::vec3 GetPosition() const { return position_; }
        void SetPosition(glm::vec3 p) override { position_ = p; }
        glm::vec3 GetFront() const override { return front_; }
        float GetNearPlane() const override { return nearPlane; }
        float GetFarPlane() const override { return farPlane; }

    private:
        void UpdateVectors()
        {
            float yR = glm::radians(yaw_), pR = glm::radians(pitch_);
            front_ = glm::normalize(glm::vec3(
                std::cos(yR) * std::cos(pR),
                std::sin(pR),
                std::sin(yR) * std::cos(pR)));
            right_ = glm::normalize(glm::cross(front_, glm::vec3(0, 1, 0)));
            up_ = glm::normalize(glm::cross(right_, front_));
        }

        glm::vec3 position_ = {0.f, 10.f, 0.f};
        float yaw_ = -90.0f;
        float pitch_ = 10.0f;
        glm::vec3 front_{0, 0, -1};
        glm::vec3 right_{1, 0, 0};
        glm::vec3 up_{0, 1, 0};
    };
}
