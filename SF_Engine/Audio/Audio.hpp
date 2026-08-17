#pragma once

#include <Engine/Engine.hpp>
#include <Controllers/CameraController.hpp>
#include <LowLevel/Rocket.hpp>
#ifdef _Platform_Mac
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

namespace SF::Engine
{
    /**
     * @brief Module used for loading, managing and playing a variety of different sound types.
     */
    class Audio : public ModuleRegistrar<Audio>
    {
        inline static const bool Registered = Register(Stage::Pre);

    public:
        enum class Type
        {
            Master,
            General,
            Effect,
            Music
        };

        Audio();
        ~Audio();

        void Update() override;

        static void CheckAl(int32_t result);

        float GetGain(Type type) const;
        void SetGain(Type type, float volume);

        /**
         * Called when a gain value has been modified.
         * @return The delegate.
         */
        rocket::signal<void(Type, float)> &OnGain()
        {
            return onGain;
        }

    private:
        // TODO: Only using p-impl because of signature differences from OpenAL and OpenALSoft.
        struct _intern;
        std::unique_ptr<_intern> impl;

        std::map<Type, float> gains;

        rocket::signal<void(Type, float)> onGain;
    };
}