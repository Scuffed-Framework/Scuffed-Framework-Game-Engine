#include <AL/al.h>

namespace SF::Engine::Audio
{
    struct ExtraAudioWaves
    {
        static constexpr ALuint Wave_Sine = 0x1001;
        static constexpr ALuint Wave_Square = 0x1002;
        static constexpr ALuint Wave_Triangle = 0x1003;
        static constexpr ALuint Wave_Sawtooth = 0x1004;
        static constexpr ALuint Wave_WhiteNoise = 0x1005;
    };
}