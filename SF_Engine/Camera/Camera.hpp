#pragma once

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
    class ACamera
    {
    public:
        virtual ~ACamera() = default;
        virtual void Update(Window *, float, bool, bool) {}

        virtual Matrix4float GetView() const = 0;
        virtual Matrix4float GetProjection(float aspect) const = 0;
        virtual Vector3float GetPosition() const = 0;
        virtual float GetNearPlane() const = 0;
        virtual float GetFarPlane() const = 0;

        virtual float GetFieldOfView() const = 0;

        virtual glm::vec3 GetFront() const = 0;
        virtual void SetPosition(glm::vec3 p) {}
        float moveSpeed = 0;
        float lookSensitivity = 0;
        float fovDeg = 0;
    };

    class Camera : public ACamera
    {
    public:
        Camera()
            : nearPlane(0.1f),
              farPlane(1000.0f),
              fieldOfView(Mathematics::Radians(45.0f)),
              viewRay(false, {0.5f, 0.5f})
        {
        }

        virtual ~Camera() = default;

        virtual void Start() {}
        virtual void Update() {}

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
        float GetFarPlane() const override
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

        Vector3float GetPosition() const override
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

        glm::vec3 GetFront() const override { return front_; }

    protected:
        float nearPlane, farPlane;
        float fieldOfView;

        Vector3float position;
        Vector3float rotation;
        Vector3float velocity;

        Matrix4float viewMatrix;
        Matrix4float projectionMatrix;

        Frustum viewFrustum;
        Ray viewRay;

        glm::vec3 front_{0, 0, -1};
    };
}