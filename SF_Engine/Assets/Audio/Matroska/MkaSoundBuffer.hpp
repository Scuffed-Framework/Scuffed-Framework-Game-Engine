#pragma once
#include <Assets/Audio/SoundBuffer.hpp>

namespace SF::Engine
{
    using namespace std;
    class MkaSoundBuffer : public SoundBuffer::Registrar<MkaSoundBuffer>
    {
        inline static const bool Registered = Register(".mka");
        friend class SoundBuffer;

    public:
        static void Load(SoundBuffer &soundBuffer, const DataInput &input);
        static void Write(const SoundBuffer &soundBuffer, const std::filesystem::path &file);
    };
} // namespace SF::Engine
