#include "AudioClip.hpp"

#ifdef _PLATFORM_MACOS
#include <OpenAL/al.h>
#else
#include <al.h>
#endif

namespace SF::Engine
{
    // Remove autism
    AudioClip::AudioClip(const std::string &filename, const Audio::Type &type, bool begin, bool loop, float gain, float pitch)
        : buffer(SoundBuffer::Create(filename)),
          type(type),
          gain(gain),
          pitch(pitch),
          source(0)
    {
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, buffer->GetBuffer());
        Audio::CheckAl(alGetError());

        SetGain(gain);
        SetPitch(pitch);

        if (begin)
            Play(loop);
    }

    // Remove autism
    AudioClip::~AudioClip()
    {
        if (source != 0)
        {
            alDeleteSources(1, &source);
            Audio::CheckAl(alGetError());
        }
    }

    void AudioClip::Play(bool loop)
    {
        alSourcei(source, AL_LOOPING, loop);
        alSourcePlay(source);
        Audio::CheckAl(alGetError());

        SetGain(gain);
    }

    void AudioClip::Pause()
    {
        if (!IsPlaying())
            return;

        alSourcePause(source);
        Audio::CheckAl(alGetError());
    }

    void AudioClip::Resume()
    {
        if (IsPlaying())
            return;

        alSourcePlay(source);
        Audio::CheckAl(alGetError());

        SetGain(gain);
    }

    void AudioClip::Stop()
    {
        if (!IsPlaying())
            return;

        alSourceStop(source);
        Audio::CheckAl(alGetError());
    }

    bool AudioClip::IsPlaying() const
    {
        ALenum state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        return state == AL_PLAYING;
    }

    void AudioClip::SetPosition(const Vec3 &position)
    {
        this->position = position;
        alSource3f(source, AL_POSITION, position.x, position.y, position.z);
        Audio::CheckAl(alGetError());
    }

    void AudioClip::SetDirection(const Vec3 &direction)
    {
        this->direction = direction;
        alSource3f(source, AL_DIRECTION, direction.x, direction.y, direction.z);
        Audio::CheckAl(alGetError());
    }

    void AudioClip::SetVelocity(const Vec3 &velocity)
    {
        this->velocity = velocity;
        alSource3f(source, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
        Audio::CheckAl(alGetError());
    }

    void AudioClip::SetGain(float gain)
    {
        this->gain = gain;
        alSourcef(source, AL_GAIN, gain * Audio::Get()->GetGain(type));
        Audio::CheckAl(alGetError());
    }

    void AudioClip::SetPitch(float pitch)
    {
        this->pitch = pitch;
        alSourcef(source, AL_PITCH, pitch);
        Audio::CheckAl(alGetError());
    }
}