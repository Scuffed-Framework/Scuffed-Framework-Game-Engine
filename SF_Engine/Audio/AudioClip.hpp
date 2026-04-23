#pragma once

#include <Math/Transform.hpp>
#include <Math/Vectors/Vector.hpp>
#include <Scene/Entity.hpp>
#include <UtilityClasses/NoCopy.hpp>
#include <memory>
#include "Audio.hpp"
#include "SoundBuffer.hpp"

namespace SF::Engine
{
    /**
     * @brief Component that represents a playable AudioClip.
     */
    struct AudioClip : NoCopy
    {
        AudioClip() = default;
        explicit AudioClip(const std::string& filename,
                           const Audio::Type& type = Audio::Type::General, bool begin = false,
                           bool loop = false, float gain = 1.0f, float pitch = 1.0f);
        ~AudioClip();

        void Play(bool loop = false);
        void Pause();
        void Resume();
        void Stop();

        bool IsPlaying() const;

        void SetPosition(const Vector3float& position);
        void SetDirection(const Vector3float& direction);
        void SetVelocity(const Vector3float& velocity);

        const Audio::Type& GetType() const
        {
            return type;
        }
        void SetType(const Audio::Type& type)
        {
            this->type = type;
        }

        float GetGain() const
        {
            return gain;
        }
        void SetGain(float gain);

        float GetPitch() const
        {
            return pitch;
        }
        void SetPitch(float pitch);

        std::shared_ptr<SoundBuffer> buffer;
        uint32_t source = 0;

        Vector3float position;
        Vector3float direction;
        Vector3float velocity;

        Audio::Type type = Audio::Type::General;
        float gain = 1.0f;
        float pitch = 1.0f;
    };

    class AudioClipSystem
    {
    public:
        static void Update(EntityRegistry& registry)
        {
            // Update all AudioClips with transforms
            auto view = registry.View<AudioClip, Transform>();
            view.each([](auto entity, AudioClip& AudioClip, Transform& transform)
                      { AudioClip.SetPosition(transform.GetPosition()); });
        }

        static void UpdateVolumes(EntityRegistry& registry)
        {
            // Update volumes when audio settings change
            auto view = registry.View<AudioClip>();
            view.each(
                [](auto entity, AudioClip& AudioClip)
                {
                    AudioClip.SetGain(
                        AudioClip.GetGain());  // This will recalculate with current Audio volume
                });
        }
    };
}
