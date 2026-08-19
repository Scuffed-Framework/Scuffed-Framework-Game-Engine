#pragma once
#include "../SoundBuffer.hpp"

namespace SF::Engine
{
    class Mp3SoundBuffer : public SoundBuffer::Registrar<Mp3SoundBuffer>
    {
        inline static const bool Registered = Register(".mp3");
        friend class SoundBuffer;

    public:
        static void Load(SoundBuffer &soundBuffer, const DataInput &input);
        static void Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file);
    };
}
