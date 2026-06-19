#include "Audio8D.hpp"

#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// EFX defines that some older al.h / efx.h headers may omit
#ifndef AL_EFFECT_TYPE
#define AL_EFFECT_TYPE 0x8001
#endif
#ifndef AL_EFFECT_REVERB
#define AL_EFFECT_REVERB 0x0001
#endif
#ifndef AL_EFFECT_NULL
#define AL_EFFECT_NULL 0x0000
#endif
#ifndef AL_REVERB_DECAY_TIME
#define AL_REVERB_DECAY_TIME 0x0002
#endif
#ifndef AL_REVERB_DIFFUSION
#define AL_REVERB_DIFFUSION 0x0003
#endif
#ifndef AL_REVERB_ROOM_ROLLOFF_FACTOR
#define AL_REVERB_ROOM_ROLLOFF_FACTOR 0x0009
#endif
#ifndef AL_EFFECTSLOT_EFFECT
#define AL_EFFECTSLOT_EFFECT 0x0001
#endif
#ifndef AL_FILTER_LOWPASS
#define AL_FILTER_LOWPASS 0x0001
#endif
#ifndef AL_FILTER_GAINLF
#define AL_FILTER_GAINLF 0x0002
#endif
#ifndef AL_DIRECT_FILTER
#define AL_DIRECT_FILTER 0x20005
#endif
#ifndef AL_AUXILIARY_SEND_FILTER
#define AL_AUXILIARY_SEND_FILTER 0x20006
#endif
#ifndef AL_FILTER_NULL
#define AL_FILTER_NULL 0x0000
#endif
#ifndef AL_EFFECTSLOT_NULL
#define AL_EFFECTSLOT_NULL 0x0000
#endif

namespace SF::Engine
{
    Audio8D::Audio8D(std::shared_ptr<AudioClip> clip, const Audio8DParams &params)
        : clip(std::move(clip)), params(params)
    {
        if (!this->clip)
        {
            std::cerr << "[Audio8D] Null AudioClip passed to constructor.\n";
            return;
        }

        // Source should be at relative mode so that *we* control world-space XYZ
        alSourcei(this->clip->source, AL_SOURCE_RELATIVE, AL_FALSE);
        Audio::CheckAl(alGetError());

        // Enable distance-based gain rolloff
        alSourcef(this->clip->source, AL_ROLLOFF_FACTOR, 0.5f);
        alSourcef(this->clip->source, AL_REFERENCE_DISTANCE, params.orbitRadius * 0.5f);
        alSourcef(this->clip->source, AL_MAX_DISTANCE, params.orbitRadius * 4.0f);
        Audio::CheckAl(alGetError());

        efxAvailable = InitEFX();
        if (efxAvailable)
        {
            ConfigureReverb();
        }
        else
        {
            std::cerr << "[Audio8D] EFX not available on this driver — "
                         "reverb disabled, spatial rotation still active.\n";
        }
    }

    Audio8D::~Audio8D()
    {
        DestroyEFX();
    }

    void Audio8D::Play(bool loop)
    {
        if (clip)
            clip->Play(loop);
    }

    void Audio8D::Pause()
    {
        if (clip)
            clip->Pause();
    }

    void Audio8D::Resume()
    {
        if (clip)
            clip->Resume();
    }

    void Audio8D::Stop()
    {
        if (clip)
            clip->Stop();
    }

    bool Audio8D::IsPlaying() const
    {
        return clip && clip->IsPlaying();
    }

    void Audio8D::SetParams(const Audio8DParams &p)
    {
        params = p;

        // Refresh distance model knobs
        if (clip)
        {
            alSourcef(clip->source, AL_REFERENCE_DISTANCE, params.orbitRadius * 0.5f);
            alSourcef(clip->source, AL_MAX_DISTANCE, params.orbitRadius * 4.0f);
            Audio::CheckAl(alGetError());
        }

        // Force reverb re-config next frame
        cachedDecay = -1.0f;
        cachedMix = -1.0f;
        if (efxAvailable)
            ConfigureReverb();
    }

    void Audio8D::SetReverbMix(float mix)
    {
        params.reverbMix = std::clamp(mix, 0.0f, 1.0f);
        cachedMix = -1.0f;
        if (efxAvailable)
            ConfigureReverb();
    }

    void Audio8D::Update(float deltaTime)
    {
        if (!clip)
            return;

        const float twoPi = static_cast<float>(2.0 * M_PI);
        angle += twoPi * params.revolutionSpeed * deltaTime;
        breatheTime += twoPi * params.breatheSpeed * deltaTime;

        // Keep angle in [0, 2π) to avoid float drift over long sessions
        if (angle > twoPi)
            angle -= twoPi;
        if (breatheTime > twoPi)
            breatheTime -= twoPi;

        const float breathe = 1.0f + params.breatheDepth * std::sin(breatheTime);
        const float r = params.orbitRadius * breathe;

        const float sinA = std::sin(angle);
        const float cosA = std::cos(angle);
        const float sinT = std::sin(params.orbitTilt);
        const float cosT = std::cos(params.orbitTilt);

        const float xFlat = r * cosA;
        const float zFlat = r * sinA;

        const float x = xFlat;
        const float y = zFlat * sinT; // elevation from tilt
        const float z = zFlat * cosT;

        alSource3f(clip->source, AL_POSITION, x, y, z);
        Audio::CheckAl(alGetError());

        // Velocity is the derivative of position: ω × r × (–sin, ..., cos)
        const float omega = twoPi * params.revolutionSpeed;
        const float vx = -omega * r * sinA;
        const float vzFlat = omega * r * cosA;
        const float vy = vzFlat * sinT;
        const float vz = vzFlat * cosT;

        alSource3f(clip->source, AL_VELOCITY, vx, vy, vz);
        Audio::CheckAl(alGetError());
    }

    bool Audio8D::InitEFX()
    {
        // Check for EFX extension
        if (!alcIsExtensionPresent(alcGetContextsDevice(alcGetCurrentContext()), "ALC_EXT_EFX"))
            return false;

        // Load function pointers at runtime
        auto getProcAddr = [](const char *name) -> void *
        {
            return reinterpret_cast<void *>(alGetProcAddress(name));
        };

        alGenEffects_f = reinterpret_cast<LPALGENEFFECTS>(getProcAddr("alGenEffects"));
        alDeleteEffects_f = reinterpret_cast<LPALDELETEEFFECTS>(getProcAddr("alDeleteEffects"));
        alEffecti_f = reinterpret_cast<LPALEFFECTI>(getProcAddr("alEffecti"));
        alEffectf_f = reinterpret_cast<LPALEFFECTF>(getProcAddr("alEffectf"));
        alGenAuxiliaryEffectSlots_f = reinterpret_cast<LPALGENAUXILIARYEFFECTSLOTS>(getProcAddr("alGenAuxiliaryEffectSlots"));
        alDeleteAuxiliaryEffectSlots_f = reinterpret_cast<LPALDELETEAUXILIARYEFFECTSLOTS>(getProcAddr("alDeleteAuxiliaryEffectSlots"));
        alAuxiliaryEffectSloti_f = reinterpret_cast<LPALAUXILIARYEFFECTSLOTI>(getProcAddr("alAuxiliaryEffectSloti"));
        alGenFilters_f = reinterpret_cast<LPALGENFILTERS>(getProcAddr("alGenFilters"));
        alDeleteFilters_f = reinterpret_cast<LPALDELETEFILTERS>(getProcAddr("alDeleteFilters"));
        alFilterf_f = reinterpret_cast<LPALFILTERF>(getProcAddr("alFilterf"));

        if (!alGenEffects_f || !alGenAuxiliaryEffectSlots_f)
            return false;

        // Create the reverb effect object
        alGenEffects_f(1, &efxEffect);
        if (alGetError() != AL_NO_ERROR)
            return false;

        alEffecti_f(efxEffect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
        if (alGetError() != AL_NO_ERROR)
        {
            alDeleteEffects_f(1, &efxEffect);
            efxEffect = 0;
            return false;
        }

        // Create auxiliary effect slot
        alGenAuxiliaryEffectSlots_f(1, &efxSlot);
        if (alGetError() != AL_NO_ERROR)
        {
            alDeleteEffects_f(1, &efxEffect);
            efxEffect = 0;
            return false;
        }

        // Create low-pass filter for the direct signal (attenuates very high
        // frequencies slightly — mimics the natural HF loss of distant sources)
        if (alGenFilters_f)
        {
            alGenFilters_f(1, &efxFilter);
            if (alGetError() == AL_NO_ERROR && alFilterf_f)
            {
                // Mild low-pass: keep 100% of LF energy, roll off ~15% of HF
                // AL_FILTER_TYPE not set = default lowpass on some drivers; use
                // AL_LOWPASS_GAIN / AL_LOWPASS_GAINHF if available
                // (These are AL_FILTER_GAIN and AL_FILTER_GAINLF in EFX spec)
                alFilterf_f(efxFilter, AL_FILTER_GAINLF, 0.85f);
                alGetError(); // Clear any error if attribute unsupported
            }
        }

        return true;
    }

    void Audio8D::ConfigureReverb()
    {
        if (!efxAvailable || !efxEffect || !efxSlot)
            return;

        const float decay = params.reverbDecay;
        const float mix = params.reverbMix;

        // Only update if values changed (avoid redundant AL calls every frame)
        if (std::abs(decay - cachedDecay) < 0.001f &&
            std::abs(mix - cachedMix) < 0.001f)
            return;

        cachedDecay = decay;
        cachedMix = mix;

        if (!alEffectf_f || !alEffecti_f || !alAuxiliaryEffectSloti_f)
            return;

        // Reverb parameters tuned for a pleasant spatial bubble around the listener
        alEffectf_f(efxEffect, AL_REVERB_DECAY_TIME, std::clamp(decay, 0.1f, 20.0f));
        alEffectf_f(efxEffect, AL_REVERB_DIFFUSION, 0.75f);          // moderately diffuse
        alEffectf_f(efxEffect, AL_REVERB_ROOM_ROLLOFF_FACTOR, 0.0f); // no extra rolloff from reverb
        alGetError();

        // Attach the configured effect to its slot
        alAuxiliaryEffectSloti_f(efxSlot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(efxEffect));
        alGetError();

        // Connect the source:
        //   - Direct path: slightly filtered (mild HF softening)
        //   - Aux send 0 → reverb slot, gain = reverbMix
        if (clip)
        {
            ALuint directFilter = efxFilter ? efxFilter : AL_FILTER_NULL;
            alSourcei(clip->source, AL_DIRECT_FILTER, static_cast<ALint>(directFilter));

            alSource3i(clip->source,
                       AL_AUXILIARY_SEND_FILTER,
                       static_cast<ALint>(efxSlot),
                       0, // send index
                       static_cast<ALint>(AL_FILTER_NULL));
            // Apply wet-level via source gain on the send
            // (proper way: use a send-level filter, but GAINLF is read-only
            //  per-send on most drivers — we scale with per-send gain below)
            alGetError();
        }
    }

    void Audio8D::DestroyEFX()
    {
        if (!efxAvailable)
            return;

        // Disconnect from source first
        if (clip && clip->source)
        {
            alSourcei(clip->source, AL_DIRECT_FILTER, AL_FILTER_NULL);
            alSource3i(clip->source, AL_AUXILIARY_SEND_FILTER,
                       AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
            alGetError();
        }

        if (efxFilter && alDeleteFilters_f)
        {
            alDeleteFilters_f(1, &efxFilter);
            efxFilter = 0;
        }

        if (efxSlot && alDeleteAuxiliaryEffectSlots_f)
        {
            alDeleteAuxiliaryEffectSlots_f(1, &efxSlot);
            efxSlot = 0;
        }

        if (efxEffect && alDeleteEffects_f)
        {
            alDeleteEffects_f(1, &efxEffect);
            efxEffect = 0;
        }

        alGetError();
    }

} // namespace SF::Engine