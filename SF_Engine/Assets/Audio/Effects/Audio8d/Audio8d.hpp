#pragma once

#include "../../AudioClip.hpp"
#include "../../Audio.hpp"

#ifdef _Platform_Mac
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <OpenAL/efx.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
#endif

#include <cmath>
#include <memory>
#include <string>

namespace SF::Engine
{
    /**
     * @brief Parameters controlling the 8D audio rotation effect.
     *
     * The "8D" effect works by continuously revolving the sound source
     * around the listener in 3D space. Combined with OpenAL's built-in
     * HRTF (Head-Related Transfer Function) and an optional reverb effect,
     * this creates strong binaural spatialisation that tricks the brain into
     * perceiving movement and depth, even on plain stereo headphones.
     *
     * Key parameters:
     *  - orbitRadius     : how far the source sits from the listener (metres)
     *  - revolutionSpeed : how fast the source spins (full revolutions per second)
     *  - orbitTilt       : elevation of the orbit plane (radians; 0 = flat horizon)
     *  - breatheDepth    : subtle radial oscillation that adds depth (0–1)
     *  - breatheSpeed    : how fast the radial oscillation cycles (Hz)
     *  - reverbMix       : wet/dry of the EFX reverb send (0 = dry, 1 = full wet)
     *  - reverbDecay     : reverb decay time in seconds
     */
    struct Audio8DParams
    {
        float orbitRadius = 3.0f;      // metres
        float revolutionSpeed = 0.12f; // rev/s  (≈ 7 RPM, feels natural, not dizzying)
        float orbitTilt = 0.18f;       // radians above horizon (~10°)
        float breatheDepth = 0.25f;    // fractional radius oscillation
        float breatheSpeed = 0.07f;    // Hz  (very slow "breathing" cycle)
        float reverbMix = 0.35f;       // reverb send level
        float reverbDecay = 1.8f;      // seconds
    };

    /**
     * @brief Wraps an AudioClip and applies a continuous 8D spatialisation effect.
     *
     * Internally this class:
     *   1. Revolves the AL source around the listener origin on each Update().
     *   2. Optionally attaches an EFX reverb auxiliary send so the brain also
     *      receives indirect reflections, reinforcing the spatial cue.
     *   3. Uses a subtle "breathe" oscillation on the orbit radius to simulate
     *      the natural fluctuation of a moving sound in free space.
     *
     * Usage:
     * @code
     *   auto clip = std::make_shared<AudioClip>("music/track.ogg", Audio::Type::Music,
     *                                           false, true); // loaded, not started, looping
     *   Audio8D effect(clip);
     *   effect.SetParams({.revolutionSpeed = 0.08f, .orbitTilt = 0.3f});
     *   effect.Play();
     *
     *   // Each frame:
     *   effect.Update(deltaTime);
     * @endcode
     */
    class Audio8D
    {
    public:
        explicit Audio8D(std::shared_ptr<AudioClip> clip,
                         const Audio8DParams &params = {});
        ~Audio8D();

        // Non-copyable, owns OpenAL EFX objects
        Audio8D(const Audio8D &) = delete;
        Audio8D &operator=(const Audio8D &) = delete;
        Audio8D(Audio8D &&) = default;

        void Play(bool loop = true);
        void Pause();
        void Resume();
        void Stop();

        bool IsPlaying() const;

        const Audio8DParams &GetParams() const { return params; }
        void SetParams(const Audio8DParams &p);

        /** Instantly jump the rotation to a given angle (radians). */
        void SetAngle(float radians) { angle = radians; }
        float GetAngle() const { return angle; }

        /** 0–1 master wet level of the EFX reverb send. */
        void SetReverbMix(float mix);

        /**
         * @brief Advance the effect by deltaTime seconds.
         * Call once per game/audio tick (e.g. from AudioClipSystem::Update).
         * @param deltaTime Seconds elapsed since last call.
         */
        void Update(float deltaTime);

    private:
        bool InitEFX();
        void DestroyEFX();
        void ConfigureReverb();

        // EFX function pointers (loaded at runtime, not all drivers expose them at link time)
        using LPALGENEFFECTS = void(AL_APIENTRY *)(ALsizei, ALuint *);
        using LPALDELETEEFFECTS = void(AL_APIENTRY *)(ALsizei, const ALuint *);
        using LPALISEFFECT = ALboolean(AL_APIENTRY *)(ALuint);
        using LPALEFFECTI = void(AL_APIENTRY *)(ALuint, ALenum, ALint);
        using LPALEFFECTF = void(AL_APIENTRY *)(ALuint, ALenum, ALfloat);
        using LPALGENAUXILIARYEFFECTSLOTS = void(AL_APIENTRY *)(ALsizei, ALuint *);
        using LPALDELETEAUXILIARYEFFECTSLOTS = void(AL_APIENTRY *)(ALsizei, const ALuint *);
        using LPALAUXILIARYEFFECTSLOTI = void(AL_APIENTRY *)(ALuint, ALenum, ALint);
        using LPALGENFILTERS = void(AL_APIENTRY *)(ALsizei, ALuint *);
        using LPALDELETEFILTERS = void(AL_APIENTRY *)(ALsizei, const ALuint *);
        using LPALFILTERF = void(AL_APIENTRY *)(ALuint, ALenum, ALfloat);
        using LPALSOURCEI = void(AL_APIENTRY *)(ALuint, ALenum, ALint);
        using LPALGETPROCADDRESS = void *(AL_APIENTRY *)(const ALchar *);

        LPALGENEFFECTS alGenEffects_f = nullptr;
        LPALDELETEEFFECTS alDeleteEffects_f = nullptr;
        LPALEFFECTI alEffecti_f = nullptr;
        LPALEFFECTF alEffectf_f = nullptr;
        LPALGENAUXILIARYEFFECTSLOTS alGenAuxiliaryEffectSlots_f = nullptr;
        LPALDELETEAUXILIARYEFFECTSLOTS alDeleteAuxiliaryEffectSlots_f = nullptr;
        LPALAUXILIARYEFFECTSLOTI alAuxiliaryEffectSloti_f = nullptr;
        LPALGENFILTERS alGenFilters_f = nullptr;
        LPALDELETEFILTERS alDeleteFilters_f = nullptr;
        LPALFILTERF alFilterf_f = nullptr;

        std::shared_ptr<AudioClip> clip;
        Audio8DParams params;

        float angle = 0.0f;       // current rotation angle (radians)
        float breatheTime = 0.0f; // internal breathe oscillator phase

        bool efxAvailable = false;
        ALuint efxEffect = 0;
        ALuint efxSlot = 0;
        ALuint efxFilter = 0;

        // Derived from Audio8DParams, stored so we can delta-update only when changed
        float cachedDecay = -1.0f;
        float cachedMix = -1.0f;
    };

} // namespace SF::Engine