// WorleyNoiseLUT.shader
// Bakes a tileable 4-octave Worley FBM into a 3-D RGBA8_UNORM texture.
// Used as the cloud base-shape noise (binding 0 in CloudRaymarch.shader).
//
// Dispatch: ceil(size/4) x ceil(size/4) x ceil(size/4)

/*
R: shape noise (Worley FBM, freq 4.0)
G: detail noise medium (Worley FBM, freq 8.0)
B: detail noise small (Worley FBM, freq 16.0)
A: coverage/weather (billowy Perlin FBM)
*/

#include "Noise/PerlinWorleyNoise.si"

// Changed from r8 to rgba8 to support four channels
[[vk::binding(0, 0)]]
[[vk::image_format("rgba8")]]
RWTexture3D<float4> outNoise;

[numthreads(4, 4, 4)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int3 coord = int3(globalThreadID.xyz);
    int3 size;
    outNoise.GetDimensions(size.x, size.y, size.z);

    if (any(coord >= size))
        return;

    float3 uvw = (float3(coord) + 0.5) / float3(size);
    float z = uvw.z;
    float freq = 4.0;

    float w0 = PerlinWorleyFBM(float3(uvw.xy, z), freq * 0.75);      // shape noise
    float w1 = PerlinWorleyFBM(float3(uvw.xy, z), freq * 2.0); // medium detail
    float w2 = PerlinWorleyFBM(float3(uvw.xy, z), freq * 4.0); // small detail
    float w3 = remap(PerlinWorleyFBM(float3(uvw.xy, z), freq * 0.5), -1.0, 1.0, 0.0, 1.0);
    w3 = remap(w3, 0.85, 1.0, 0.0, 1.0); // fake cloud coverage

    // Pack into RGBA channels
    float4 pw = float4(w0, w1, w2, w3);

    outNoise[coord] = pw;
}