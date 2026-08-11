#include "Clouds/CloudCommon.si"
#include "Noise/BlueNoise.si"

// Evaluate quarter resolution.
[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 texSize;
    imageCloudRenderTexture.GetDimensions(texSize.x, texSize.y);
    int2 workPos = int2(dispatchThreadID.xy);

    if(workPos.x >= texSize.x || workPos.y >= texSize.y)
    {
        return;
    }

    uint bayerIndex = kCloud.frameIndex.x % 16;
    int bayerValue = kBayer4x4[bayerIndex];
    int2 bayerOffset = int2(bayerValue % 4, bayerValue / 4);

    int2 fullResSize = texSize * 4;
    int2 fullResWorkPos = workPos * 4 + int2(bayerOffset);

    // Get evaluate uv in full resolution.
    const float2 uv = (float2(fullResWorkPos) + float2(0.5f)) / float2(fullResSize);

    // FIXED: Use the blue noise function you have available
    float blueNoise = BlueNoiseErrorDistrib(workPos.x, workPos.y, 0, 0u);

    // Offset retarget for new seeds each frame
    uint2 offset = uint2(float2(0.754877669, 0.569840296) * (kCloud.frameIndex.x) * uint2(texSize));
    uint2 offsetId = workPos.xy + offset;
    offsetId.x = offsetId.x % texSize.x;
    offsetId.y = offsetId.y % texSize.y;
    float blueNoise2 = BlueNoiseErrorDistrib(offsetId.x, offsetId.y, 0, 0u);

    float4 clipSpace = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.5, 1.0);
    float4 viewPosH = mul(kAtmo.invProj, clipSpace);
    float3 viewSpaceDir = normalize(float3(clipSpace.x / kAtmo.invProj[0][0],
                                           clipSpace.y / kAtmo.invProj[1][1],
                                           -1.0));
    float3 rayDir = normalize(mul((float3x3)kAtmo.invView, viewSpaceDir));

    float depth = 0.0; // reverse z.
    float4 fogLighting = float4(0.0, 0.0, 0.0, 0.0);

    float4 cloudColor = cloudColorCompute(kCloud, kAtmo, uv, blueNoise2, depth, workPos, rayDir, true, fogLighting, blueNoise2);

    if (anyBadFloat(depth))
    {
        depth = 0.0;
    }

    imageCloudRenderTexture[workPos] = cloudColor;
    imageCloudDepthTexture[workPos] = float4(depth, 0.0, 0.0, 0.0);
    imageCloudFogRenderTexture[workPos] = fogLighting;
}