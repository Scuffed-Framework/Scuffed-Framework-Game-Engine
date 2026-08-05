#pragma once

#include <Math/BasicMath.hpp>
#include <array>
#include <cstdint>

namespace SF::Engine
{
    // Mirrors HLSL `SpectrumParameters` from the Acerola FFT-Ocean reference
    // (gasgiant/FFT-Ocean). Two per cascade (local + swell), 4 cascades = 8.
    struct SpectrumParameters
    {
        float scale = 1.0f;
        float angle = 0.0f;
        float spreadBlend = 1.0f;
        float swell = 0.5f;
        float alpha = 0.0002f;
        float peakOmega = 1.0f;
        float gamma = 1.0f;
        float shortWavesFade = 0.01f;
    };
    static_assert(sizeof(SpectrumParameters) == 32, "std430 layout mismatch");

    // One cascade = one tile size / frequency band. 4 cascades cover the
    // range from large swells (LengthScale0, e.g. 1000m) down to capillary
    // ripples (LengthScale3, e.g. 5-10m). N is the FFT resolution per
    // cascade (must be 1024 to match FFT_SIZE in OceanFFTButterfly.si).
    struct OceanFFTSettings
    {
        uint32_t N = 1024;

        float lengthScale0 = 1000.0f;
        float lengthScale1 = 250.0f;
        float lengthScale2 = 50.0f;
        float lengthScale3 = 10.0f;

        float gravity = 9.81f;
        float depth = 500.0f;
        float repeatTime = 200.0f; // seconds before the FFT field tiles/loops
        float damping = 0.001f;

        float lowCutoff = 0.0001f;
        float highCutoff = 9000.0f;

        // Choppiness (per-axis displacement scale, Tessendorf's "lambda").
        glm::vec2 lambda = glm::vec2(1.0f, 1.0f);
        glm::vec2 normalStrength = glm::vec2(1.0f, 1.0f);

        int seed = 12345;

        // Foam accumulation (jacobian-based, Acerola/gasgiant model).
        float foamBias = 1.0f;
        float foamDecayRate = 0.05f;
        float foamAdd = 0.5f;
        float foamThreshold = 0.0f;

        glm::vec2 wind = glm::vec2(1.0f, 1.0f);

        // 8 entries: [cascade][0]=local wind sea, [cascade][1]=swell.
        // scale=0 on the swell entry disables it for that cascade.
        std::array<SpectrumParameters, 8> spectrums = []()
        {
            std::array<SpectrumParameters, 8> s{};
            for (int i = 0; i < 4; ++i)
            {
                s[i * 2].scale = 1.0f;
                s[i * 2].alpha = 0.0002f - i * 0.00003f;
                s[i * 2].peakOmega = 0.8f + i * 0.6f;
                s[i * 2].gamma = 1.0f;
                s[i * 2].shortWavesFade = 0.01f + i * 0.05f;
                s[i * 2].spreadBlend = 1.0f;

                s[i * 2 + 1].scale = (i < 2) ? 1.0f : 0.0f; // swell only on large cascades
                s[i * 2 + 1].alpha = 0.0001f;
                s[i * 2 + 1].peakOmega = 0.5f;
                s[i * 2 + 1].gamma = 5.0f;
                s[i * 2 + 1].swell = 0.8f;
                s[i * 2 + 1].shortWavesFade = 0.005f;
            }
            return s;
        }();
    };

    // Matches OceanFFTCommon.si SpectrumUBO (std140).
    struct OceanFFTSpectrumUBO
    {
        float frameTime, deltaTime, A, gravity;
        float repeatTime, damping, depth, lowCutoff;
        float highCutoff;
        int seed;
        float _pad0, _pad1;
        glm::vec2 wind;           // 8
        glm::vec2 lambda;         // 8
        glm::vec2 normalStrength; // 8
        glm::vec2 _pad2;
        uint32_t N, lengthScale0, lengthScale1, lengthScale2;
        uint32_t lengthScale3;
        float foamBias, foamDecayRate, foamAdd;
        float foamThreshold;
    };
}