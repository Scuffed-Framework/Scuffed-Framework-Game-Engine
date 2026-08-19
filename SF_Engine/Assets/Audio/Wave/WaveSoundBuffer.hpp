#pragma once
#include "../SoundBuffer.hpp"

namespace SF::Engine
{
    class WaveSoundBuffer : public SoundBuffer::Registrar<WaveSoundBuffer>
    {
        inline static const bool Registered = Register(".wav", ".wave");
        friend class SoundBuffer;

    public:
        static void Load(SoundBuffer &soundBuffer, const DataInput &input);
        static void Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file);
    };
}