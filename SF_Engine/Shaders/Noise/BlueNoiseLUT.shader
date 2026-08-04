#include "Noise/BlueNoise.si"

[[vk::binding(0, 0)]]
[[vk::image_format("r16f")]]
RWTexture2D<float4> outBlueNoise;

[numthreads(8, 8, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int2 coord = int2(globalThreadID.xy);
    int2 size;
    outBlueNoise.GetDimensions(size.x, size.y);
    if (coord.x >= size.x || coord.y >= size.y) return;

    float2 uv = float2(coord) / float2(size);
    float blueNoise = BlueNoiseErrorDistrib(
        uint(coord.x), 
        uint(coord.y), 
        0, 
        0u
    );

    outBlueNoise[coord] = float4(blueNoise, 0.0, 0.0, 1.0);
}