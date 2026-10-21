#pragma once
#include <Assets/Audio/AudioClip.hpp>
#include <Assets/Audio/Waves.hpp>
#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>

namespace SF::Engine
{
    using namespace std;
    class AudioTests
    {
        using Clock = std::chrono::steady_clock;

    public:
        struct ToneState
        {
            float freq     = 440.0f;
            float duration = 0.5f;
        };

        struct MetronomeState
        {
            bool running     = false;
            float bpm        = 120.0f;
            int beatsPerBar  = 4;
            bool accentFirst = true;
            float pitch      = 1000.0f; // Hz, accent is 1.5x this

            int currentBeat = 0; // beat that was played most recently
            TimePoint nextBeat{};

            TimePoint lastBeat{};
            std::vector<TimePoint> taps;
        };

        ToneState gTone;
        MetronomeState gMetro;
        bool gShowAudioTools = false;

        void PlayClick(bool accent, float pitch)
        {
            // Buffer is kept alive alongside the clip, same as BarPanels::TestAudio does.
            static decltype(SoundBuffer::CreateWave(ExtraAudioWaves::Wave_Sine, 1.0f, 1.0f)) clickBuffer;
            static std::shared_ptr<AudioClip> clickClip;

            clickClip.reset();
            clickBuffer = SoundBuffer::CreateWave(ExtraAudioWaves::Wave_Sine, accent ? pitch * 1.5f : pitch, 0.04f);
            clickClip   = std::make_shared<AudioClip>(clickBuffer);
            clickClip->SetEnabled(true);
            clickClip->Play();
        }

        void StartMetronome()
        {
            gMetro.running     = true;
            gMetro.currentBeat = gMetro.beatsPerBar - 1; // so the first click lands on beat 1
            gMetro.nextBeat    = Clock::now();
        }

        void StopMetronome() { gMetro.running = false; }

        // Called every frame. Uses an absolute schedule (nextBeat += interval) so tempo
        // doesn't drift; click timing is still quantised to the frame rate.
        void UpdateMetronome()
        {
            auto &m = gMetro;
            if (!m.running)
                return;

            const auto now = Clock::now();
            if (now < m.nextBeat)
                return;

            const auto interval =
                    std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(60.0 / m.bpm));

            m.currentBeat = (m.currentBeat + 1) % std::max(1, m.beatsPerBar);
            PlayClick(m.accentFirst && m.currentBeat == 0, m.pitch);
            m.lastBeat = now;
            m.nextBeat += interval;

            // If we stalled (breakpoint, long frame), resync instead of firing a burst of clicks.
            if (m.nextBeat < now)
                m.nextBeat = now + interval;
        }

        void TapTempo()
        {
            auto &m        = gMetro;
            const auto now = Clock::now();

            if (!m.taps.empty() && now - m.taps.back() > std::chrono::seconds(2))
                m.taps.clear();

            m.taps.push_back(now);
            if (m.taps.size() > 8)
                m.taps.erase(m.taps.begin());

            if (m.taps.size() >= 2)
            {
                const double span = std::chrono::duration<double>(m.taps.back() - m.taps.front()).count();
                if (span > 0.0)
                    m.bpm = static_cast<float>(
                            std::clamp(60.0 * static_cast<double>(m.taps.size() - 1) / span, 20.0, 300.0));
            }
        }
    };
} // namespace SF::Engine
