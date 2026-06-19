#include "Audio.hpp"

#include <Scene/SceneManager.hpp>
#include <iomanip>

namespace SF::Engine
{
    struct Audio::_intern
    {
        ALCdevice *device = nullptr;
        ALCcontext *context = nullptr;
    };

    Audio::Audio() : impl(std::make_unique<_intern>())
    {
        impl->device = alcOpenDevice(nullptr);
        if (!impl->device)
        {
            Log::Error("Failed to open OpenAL device");
            return;
        }

        impl->context = alcCreateContext(impl->device, nullptr);
        if (!impl->context)
        {
            Log::Error("Failed to create OpenAL context");
            alcCloseDevice(impl->device);
            return;
        }

        alcMakeContextCurrent(impl->context);

        // List available audio devices
        auto devices = alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
        auto device = devices;
        auto next = devices + 1;

        while (device && *device != '\0' && next && *next != '\0')
        {
            Log::Info("Audio Device: {}", device);
            auto len = std::strlen(device);
            device += len + 1;
            next += len + 2;
        }

        auto deviceName = alcGetString(impl->device, ALC_DEVICE_SPECIFIER);
        Log::Info("Selected Audio Device: \"{}\"", deviceName);
    }

    Audio::~Audio()
    {
        alcMakeContextCurrent(nullptr);
        if (impl->context)
            alcDestroyContext(impl->context);
        if (impl->device)
            alcCloseDevice(impl->device);
    }

    void Audio::Update()
    {
        auto scene = SceneManager::Get()->GetScene();
        if (!scene)
            return;

        auto camera = scene->GetCamera();
        if (!camera)
            return;

        // Set listener gain
        alListenerf(AL_GAIN, GetGain(Type::Master));

        // Set listener position
        auto *cam = CameraController::Get().GetActive();
        auto position = cam->GetPosition();
        alListener3f(AL_POSITION, position.x, position.y, position.z);

        // Set listener velocity  ACamera doesn't track velocity, so use zero
        alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);

        // Set listener orientation using front and a fixed world-up
        auto front = cam->GetFront();
        ALfloat orientation[6] = {
            front.x, front.y, front.z, // Forward vector
            0.0f, 1.0f, 0.0f           // Up vector
        };
        alListenerfv(AL_ORIENTATION, orientation);
    }

    void Audio::CheckAl(int32_t error)
    {
        if (error == AL_NO_ERROR)
            return;

        const char *errorStr = "Unknown Error";
        switch (error)
        {
        case AL_INVALID_NAME:
            errorStr = "Invalid Name";
            break;
        case AL_INVALID_ENUM:
            errorStr = "Invalid Enum";
            break;
        case AL_INVALID_VALUE:
            errorStr = "Invalid Value";
            break;
        case AL_INVALID_OPERATION:
            errorStr = "Invalid Operation";
            break;
        case AL_OUT_OF_MEMORY:
            errorStr = "Out of Memory";
            break;
        }

        Log::Error("OpenAL Error: {} ({})", errorStr, error);
    }

    float Audio::GetGain(Type type) const
    {
        if (auto it = gains.find(type); it != gains.end())
            return it->second;
        return 1.0f;
    }

    void Audio::SetGain(Type type, float volume)
    {
        // Clamp volume to valid range
        volume = std::clamp(volume, 0.0f, 1.0f);

        auto it = gains.find(type);
        if (it != gains.end())
        {
            it->second = volume;
        }
        else
        {
            gains.emplace(type, volume);
        }

        onGain(type, volume);
    }
}