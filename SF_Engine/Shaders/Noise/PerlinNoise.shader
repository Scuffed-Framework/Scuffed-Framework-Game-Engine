#include "Noise/PerlinNoise.si"

[[vk::binding(0, 0)]]
[[vk::image_format("r8")]]
RWTexture2D<float4> perlinNoiseTex;

[numthreads(8, 8, 1)]
void main(uint3 globalThreadID: SV_DispatchThreadID)
{
    int2 uv = int2(globalThreadID.xy);

    // get texture size
    int2 size;
    perlinNoiseTex.GetDimensions(size.x, size.y);
    if (uv.x >= size.x || uv.y >= size.y)
        return;

    // Normalized coords
    float2 p = float2(uv) / float2(size);

    // Turn 2D -> 3D Perlin input
    float3 pos = float3(p * 8.0, 0.0);

    float n = perlin(pos) * 0.5 + 0.5; // remap to 0–1

    perlinNoiseTex[uv] = float4(n, 0.0, 0.0, 1.0);
}
