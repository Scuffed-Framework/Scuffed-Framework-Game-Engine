#include "Noise/WorleyNoise.si"

#define kDetailFrequency 8.0

[[vk::binding(0, 0)]]
RWTexture3D<float> imageWorleyNoise;

[numthreads(8, 8, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int3 texSize;
    imageWorleyNoise.GetDimensions(texSize.x, texSize.y, texSize.z);
    int3 workPos = int3(globalThreadID.xyz);

    if (workPos.x >= texSize.x || workPos.y >= texSize.y || workPos.z >= texSize.z)
    {
        return;
    }

    const float3 uvw = (float3(workPos) + float3(0.5f)) / float3(texSize);

    float detailNoise = 
        worleyFbm(uvw, kDetailFrequency * 1.0, 4) * 0.625 +
        worleyFbm(uvw, kDetailFrequency * 2.0, 4) * 0.250 +
        worleyFbm(uvw, kDetailFrequency * 4.0, 4) * 0.125;

    imageWorleyNoise[workPos] = detailNoise;
}