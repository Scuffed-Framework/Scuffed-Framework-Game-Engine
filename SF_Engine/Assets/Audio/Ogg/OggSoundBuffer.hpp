#pragma once
#include "../SoundBuffer.hpp"

namespace SF::Engine
{
    class OggSoundBuffer : public SoundBuffer::Registrar<OggSoundBuffer>
    {
        inline static const bool Registered = Register(".ogg", ".OGG");
        friend class SoundBuffer;

    public:
        static void Load(SoundBuffer &soundBuffer, const DataInput &input);
    };
} // namespace SF::Engine
