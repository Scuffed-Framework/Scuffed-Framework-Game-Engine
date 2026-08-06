#include "Noise/WorleyNoise.si"
#include "Noise/PerlinNoise.si"

#define kBasicFrequency 4.0
#define kBasicNoiselerpFactor 0.5

[[vk::binding(0, 0)]]
[[vk::image_format("r8")]]
RWTexture3D<float4> imageBasicNoise;

float remap(float x, float a, float b, float c, float d)
{
    return (((x - a) / (b - a)) * (d - c)) + c;
}

float basicNoiseComposite(float4 v)
{
    float wfbm = v.y * 0.625 + v.z * 0.25 + v.w * 0.125; 
    return remap(v.x, wfbm - 1.0, 1.0, 0.0, 1.0);
}

[numthreads(8, 8, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int3 texSize;
    imageBasicNoise.GetDimensions(texSize.x, texSize.y, texSize.z);
    int3 workPos = int3(globalThreadID.xyz);

    if (workPos.x >= texSize.x || workPos.y >= texSize.y || workPos.z >= texSize.z)
    {
        return;
    }

    const float3 uvw = (float3(workPos) + float3(0.5f)) / float3(texSize);

    float pfbm = lerp(1.0, perlinfbm(uvw, kBasicFrequency, 7), kBasicNoiselerpFactor);
    pfbm = abs(pfbm * 2.0 - 1.0); // billowy perlin noise
    
    float4 col = float4(0.0);
    col.g += worleyFbm(uvw, kBasicFrequency * 1.0, 4);
    col.b += worleyFbm(uvw, kBasicFrequency * 2.0, 4);
    col.a += worleyFbm(uvw, kBasicFrequency * 4.0, 4);

    col.r += remap(pfbm, 0.0, 1.0, col.g, 1.0); // perlin-worley
    
    imageBasicNoise[workPos] = float4(basicNoiseComposite(col));
}