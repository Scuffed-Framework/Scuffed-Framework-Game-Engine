#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <XML/XMLReader.hpp>
#include <Scene/SceneSerialization.hpp>

namespace SF::Engine
{
    /**
     * @brief CPU-side transform: position, euler rotation (degrees), scale.
     *
     * Matches Unity's Transform component layout.
     * ToMatrix() builds model = T * Ry * Rx * Rz * S  (YXZ euler order).
     */
    struct TransformComponent : public Serializable
    {
        glm::vec3 position = {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation = {0.0f, 0.0f, 0.0f}; // degrees, XYZ euler
        glm::vec3 scale = {1.0f, 1.0f, 1.0f};

        glm::mat4 ToMatrix() const
        {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
            glm::mat4 R = glm::yawPitchRoll(
                glm::radians(rotation.y),
                glm::radians(rotation.x),
                glm::radians(rotation.z));
            glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
            return T * R * S;
        }

        void Reset()
        {
            position = {0.0f, 0.0f, 0.0f};
            rotation = {0.0f, 0.0f, 0.0f};
            scale = {1.0f, 1.0f, 1.0f};
        }
        void Serialize(XMLNode &node) const override
        {
            SerializeVec3(node, "position", position);
            SerializeVec3(node, "rotation", rotation);
            SerializeVec3(node, "scale", scale);
        }

        void Deserialize(const XMLNode &node) override
        {
            position = DeserializeVec3(node, "position");
            rotation = DeserializeVec3(node, "rotation");
            scale = DeserializeVec3(node, "scale", {1.f, 1.f, 1.f});
        }
    };
}
