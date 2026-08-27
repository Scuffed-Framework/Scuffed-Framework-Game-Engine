#pragma once

#include <Math/Transform.hpp>
#include <Math/Vectors/Vector.hpp>
#include <Entity/Entity.hpp>
#include <Entity/Components/Component.hpp>
#include <memory>
#include "Audio.hpp"
#include "SoundBuffer.hpp"
#include "Waves.hpp"

namespace SF::Engine
{
    // TODO: ADD A WAY TO CREATE FROM MEMORY OR FROM THE STUFF IN WAVES.HPP
    /**
     * @brief Component that represents a playable AudioClip.
     */
    class AudioClip : public Component::Registrar<AudioClip>
    {
        inline static const bool Registered = Register("audioClip");

    public:
        AudioClip() = default;
        explicit AudioClip(const DataInput &input,
                           const Audio::Type &type = Audio::Type::General, bool begin = false,
                           bool loop = false, float gain = 1.0f, float pitch = 1.0f);

        explicit AudioClip(std::shared_ptr<SoundBuffer> buffer,
                   const Audio::Type &type = Audio::Type::General, bool begin = false,
                   bool loop = false, float gain = 1.0f, float pitch = 1.0f);

        AudioClip(const AudioClip &) = delete;
        AudioClip &operator=(const AudioClip &) = delete;
        AudioClip(AudioClip &&other) noexcept;
        AudioClip &operator=(AudioClip &&other) noexcept;
                   
        ~AudioClip();

        void Play(bool loop = false);
        void Pause();
        void Resume();
        void Stop();

        bool IsPlaying() const;

        void SetPosition(const Vec3 &position);
        void SetDirection(const Vec3 &direction);
        void SetVelocity(const Vec3 &velocity);

        const Audio::Type &GetType() const { return type; }
        void SetType(const Audio::Type &type) { this->type = type; }

        float GetGain() const { return gain; }
        void SetGain(float gain);

        float GetPitch() const { return pitch; }
        void SetPitch(float pitch);

        std::shared_ptr<SoundBuffer> buffer;
        uint32_t source = 0;

        Vec3 position;
        Vec3 direction;
        Vec3 velocity;

        Audio::Type type = Audio::Type::General;
        float gain = 1.0f;
        float pitch = 1.0f;
        float length;
    };

    class AudioClipSystem
    {
    public:
        static void Update(EntityRegistry &registry)
        {
            registry.ForEach([](Entity *entity)
                             {
                auto* clip = entity->GetComponent<AudioClip>();
                auto* transform = entity->GetComponent<Transform>();
                if (clip && transform)
                {
                    clip->SetPosition(transform->GetPosition());
                } });
        }

        static void UpdateVolumes(EntityRegistry &registry)
        {
            registry.ForEach([](Entity *entity)
                             {
                if (auto* clip = entity->GetComponent<AudioClip>())
                {
                    clip->SetGain(clip->GetGain()); // recompute with current Audio volume
                } });
        }
    };
}