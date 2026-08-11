// R = coverage, G = cloud height/thickness, B = cloud type (0=stratus, 1=cumulus)
#include "Noise/PerlinWorleyNoise.si"
[[vk::binding(0, 0)]] [[vk::image_format("rgba8")]]
RWTexture2D<float4> outWeatherMap;

cbuffer WeatherParams : register(b1)
{
    uint  resolution;      // e.g. 512-1024, this tiles across a huge world area
    float coverageScale;   // frequency of the coverage blobs
    float heightScale;     // usually << coverageScale, height varies more smoothly
    float typeScale;       // lowest frequency of the three — whole regions share a type
    float coverageBias;    // shifts overall coverage up/down, tune in your UI
};

// Perlin-Worley FBM, same style as your PerlinWorleyNoiseLUT — reuse that noise
// function directly rather than duplicating it here.
float fbm(float2 p, int octaves)
{
    float sum = 0.0, amp = 0.5, freq = 1.0;
    sum += amp * PerlinWorleyFBM(float3(p * freq, 1), octaves);
    freq *= 2.0;
    amp *= 0.5;
    return sum;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= resolution)) return;

    float2 uv = (float2(id.xy) + 0.5) / float(resolution);

    // R: coverage — high-frequency FBM, worley-heavy for the round blobby clusters
    // visible in your reference image. Erode toward zero so black gaps stay clear.
    float coverage = fbm(uv * coverageScale, 5);
    coverage = saturate(coverage + coverageBias);
    coverage = pow(coverage, 1.4); // sharpen edges of the blobs vs a flat gradient

    // G: height — much lower frequency than coverage, height doesn't jitter
    // texel-to-texel the way coverage does, it varies over whole cloud systems.
    float height = fbm(uv * heightScale, 3) * 0.5 + 0.5;

    // B: type — lowest frequency of all three. Large uniform regions of stratus
    // vs cumulus, matching how whole weather fronts share a character.
    float type = fbm(uv * typeScale, 2) * 0.5 + 0.5;

    outWeatherMap[id.xy] = float4(coverage, height, type, 1.0);
}