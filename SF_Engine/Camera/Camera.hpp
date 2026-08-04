#pragma once

#include <Components/Component.hpp>
#include <Math/Math.hpp>
#include <Physics/Frustum.hpp>
#include <Physics/Ray.hpp>
#include <Math/Math.hpp>
#include <Graphics/Windows/Window.hpp>

namespace SF::Engine
{
    /**
     * @brief Component that represents a scene camera, this object should be overridden with new
     * behaviour.
     */

    class Camera : public Component::Registrar<Camera>
    {
    public:
        Camera()
            : nearPlane(0.1f),
              farPlane(1000.0f),
              fieldOfView(Mathematics::Radians(45.0f)),
              viewRay(false, {0.5f, 0.5f}),
              inverseZ(true),
              infiniteFarPlane(true)
        {
        }

        Camera(bool inverseZ)
            : nearPlane(0.1f),
              farPlane(1000.0f),
              fieldOfView(Mathematics::Radians(45.0f)),
              viewRay(false, {0.5f, 0.5f}),
              inverseZ(inverseZ)
        {
        }
        Camera(bool inverseZ, bool infFarPlane)
            : nearPlane(0.1f),
              farPlane(1000.0f),
              fieldOfView(Mathematics::Radians(45.0f)),
              viewRay(false, {0.5f, 0.5f}),
              inverseZ(inverseZ),
              infiniteFarPlane(infFarPlane)
        {
        }
        TypeId GetTypeId() const override
        {
            return TypeInformation<Component>::GetTypeId<Camera>();
        }

        std::string_view GetTypeName() const override
        {
            return TypeInformation<Component>::GetTypeName<Camera>();
        }

        virtual void Update(Window *, float /*DeltaTime*/, bool /*imguiWantsMouse*/, bool /*imguiWantsKeyboard*/) = 0;
        virtual void Start() {}
        /**
         * Gets the distance of the near pane of the view frustum.
         * @return The distance of the near pane of the view frustum.
         */
        float GetNearPlane() const
        {
            return nearPlane;
        }
        void SetNearPlane(float nearPlane)
        {
            this->nearPlane = nearPlane;
        }

        /**
         * Gets the distance of the view frustum's far plane.
         * @return The distance of the view frustum's far plane.
         */
        float GetFarPlane() const
        {
            return farPlane;
        }
        void SetFarPlane(float farPlane)
        {
            this->farPlane = farPlane;
        }

        /**
         * Gets the field of view angle for the view frustum.
         * @return The field of view angle for the view frustum.
         */
        float GetFieldOfView() const
        {
            return fieldOfView;
        }
        void SetFieldOfView(float fieldOfView)
        {
            this->fieldOfView = fieldOfView;
        }

        Vector3float GetPosition() const
        {
            return position;
        }

        const Vector3float &GetRotation() const
        {
            return rotation;
        }
        const Vector3float &GetVelocity() const
        {
            return velocity;
        }

        /**
         * Gets the view matrix created by the current camera position and rotation.
         * @return The view matrix created by the current camera position and rotation.
         */
        const Matrix4float &GetViewMatrix() const
        {
            return viewMatrix;
        }

        /**
         * Gets the projection matrix used in the current scene render.
         * @return The projection matrix used in the current scene render.
         */
        const Matrix4float &GetProjectionMatrix() const
        {
            return projectionMatrix;
        }

        /**
         * Gets the view frustum created by the current camera position and rotation.
         * @return The view frustum created by the current camera position and rotation.
         */
        const Frustum &GetViewFrustum() const
        {
            return viewFrustum;
        }

        /**
         * Gets the ray that extends from the cameras position though the screen.
         * @return The cameras view ray.
         */
        const Ray &GetViewRay() const
        {
            return viewRay;
        }

        glm::vec3 GetFront() const { return front_; }

        /**
        @brief inverse depth buffer and infinite far plane,
        no way to turn off inv z and infinite depth
        @claude fix this
        */
        bool IsInverseZ() const { return inverseZ; }
        void SetInverseZ(bool value) { inverseZ = value; }
        bool IsFarPlaneInfinite() const { return infiniteFarPlane; }
        void SetFarPlaneInfinite(bool value) { infiniteFarPlane = value; }

        Matrix4float GetProjection(float aspect) const
        {
            if (infiniteFarPlane)
                return InfiniteFarProjection(aspect);

            glm::mat4 p = glm::perspective(glm::radians(fovDeg), aspect, nearPlane, farPlane);
            p[1][1] *= -1.0f; // flip y cuz vulkan yk
            return p;
        }

    protected:
        float nearPlane, farPlane;
        float fieldOfView;
        bool inverseZ;
        bool infiniteFarPlane;

        Vector3float position;
        Vector3float rotation;
        Vector3float velocity;

        Matrix4float viewMatrix;
        Matrix4float projectionMatrix;

        Frustum viewFrustum;
        Ray viewRay;

    public:
        void SetPosition(glm::vec3 p) { position = p; }

        glm::mat4 InfiniteFarProjection(float aspect) const
        {
            float f = 1.0f / std::tan(glm::radians(fovDeg) * 0.5f);
            glm::mat4 p(0.0f);
            p[0][0] = f / aspect;
            p[1][1] = f;
            p[2][3] = -1.0f;

            if (inverseZ)
            {
                // depth 1 at near, 0 at infinity
                p[2][2] = 0.0f;
                p[3][2] = nearPlane;
            }
            else
            {
                // depth 0 at near, 1 at infinity
                p[2][2] = -1.0f;
                p[3][2] = -nearPlane;
            }

            p[1][1] *= -1.0f; // Vulkan Y-flip
            return p;
        }

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

        glm::mat4 GetView() const
        {
            // Build view matrix from basis vectors directly instead of lookAt(eye, eye+dir, up).
            // lookAt(position_, position_ + front_, up) loses precision at large |position_|
            // because front_ (magnitude ~1) gets swallowed into position_'s float mantissa,
            // causing the "target" to jump between discrete representable values -> jittery rotation.
            glm::vec3 f = glm::normalize(front_);
            glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0, 1, 0)));
            glm::vec3 u = glm::cross(r, f);

            glm::mat4 view(1.0f);
            view[0][0] = r.x;
            view[1][0] = r.y;
            view[2][0] = r.z;
            view[0][1] = u.x;
            view[1][1] = u.y;
            view[2][1] = u.z;
            view[0][2] = -f.x;
            view[1][2] = -f.y;
            view[2][2] = -f.z;
            view[3][0] = -glm::dot(r, position);
            view[3][1] = -glm::dot(u, position);
            view[3][2] = glm::dot(f, position);

            return view;
        }

        float yaw_ = -90.0f;
        float pitch_ = 10.0f;
        glm::vec3 front_{0, 0, -1};
        glm::vec3 right_{1, 0, 0};
        glm::vec3 up_{0, 1, 0};
        float fovDeg = 60.0f;
        float minFov = 10.0f;
        float maxFov = 120.0f;
    };
}