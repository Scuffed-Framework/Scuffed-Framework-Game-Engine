#pragma once
#include "../SoundBuffer.hpp"

namespace SF::Engine
{
    class OpusSoundBuffer : public SoundBuffer::Registrar<OpusSoundBuffer>
    {
        inline static const bool Registered = Register(".ogg", ".mka"); // if audio encoding is opus
        friend class SoundBuffer;

    public:
        static void Load(SoundBuffer &soundBuffer, const DataInput &input);
        static void Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file);
    };
}