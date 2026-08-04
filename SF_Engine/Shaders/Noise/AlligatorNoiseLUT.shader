#include "Noise/PerlinWorleyNoise.si"
#include "Noise/AlligatorNoise.si"

[[vk::binding(0, 0)]]
[[vk::image_format("rgba8")]]
RWTexture3D<float4> outNoise;

const static float kCurlStrength = 0.3;

[numthreads(4, 4, 4)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int3 coord = int3(globalThreadID.xyz);
    int3 size;
    outNoise.GetDimensions(size.x, size.y, size.z);

    if (any(coord >= size))
        return;

    float3 uvw = (float3(coord) + 0.5) / float3(size);

    // R: low-frequency curly-alligator (large wispy web shapes)
    float ca_lo = curlyAlligatorFBM(uvw, 4.0, kCurlStrength);

    // G: high-frequency curly-alligator (fine wispy wisps)
    float ca_hi = curlyAlligatorFBM(uvw, 8.0, kCurlStrength * 0.6);

    // B: low-frequency alligator (large billows, invertCloud=true)
    float al_lo = alligatorFBM(uvw, 4.0, true);

    // A: high-frequency alligator (fine billows, invertCloud=true)
    float al_hi = alligatorFBM(uvw, 8.0, true);

    outNoise[coord] = float4(ca_lo, ca_hi, al_lo, al_hi);
}