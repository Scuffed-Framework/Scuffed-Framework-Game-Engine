#pragma once
#include "../SoundBuffer.hpp"

namespace SF::Engine
{
    class FlacSoundBuffer : public SoundBuffer::Registrar<FlacSoundBuffer>
    {
        inline static const bool Registered = Register(".flac");
        friend class SoundBuffer;

    public:
        static void Load(SoundBuffer &soundBuffer, const DataInput &input);
        static void Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file);
    };
}
