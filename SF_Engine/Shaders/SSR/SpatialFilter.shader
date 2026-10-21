// SpatialFilter.shader — Probed Stochastic SSR, stage 4/5.
//
// Edge-avoiding (depth + normal weighted) disk-sampled blur over the
// temporally accumulated reflection buffer. Kernel radius grows with
// surface roughness (a rough surface's reflection is low-frequency, so it
// tolerates — and benefits from — a wider blur) and with the estimated
// variance from TemporalAccumulate's moments (noisier regions get a wider
// kernel, converged regions stay sharp). Sample positions are rotated per
// pixel with a blue-noise angle so the fixed 8-tap pattern doesn't leave
// visible ring artifacts.
//
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0   ConstantBuffer SSRParams
//   binding 1   Texture2D      gbufDepth
//   binding 2   Texture2D      gbufNormal
//   binding 3   Texture2D      gbufPBR
//   binding 4   Texture2D      inAccumColor    (from TemporalAccumulate, or Trace directly if temporal disabled)
//   binding 5   Texture2D      inAccumMoments
//   binding 6   RWTexture2D<float4> imgFiltered (rgb=denoised, a=confidence)
//   binding 31  ConstantBuffer Camera (shared)
#include "SSR/SSRCommon.si"
#include "Common/Samplers.si"

[[vk::binding(0, 0)]] ConstantBuffer<SSRParams> kSSR;
[[vk::binding(1, 0)]] Texture2D gbufDepth;
[[vk::binding(2, 0)]] Texture2D gbufNormal;
[[vk::binding(3, 0)]] Texture2D gbufPBR;
[[vk::binding(4, 0)]] Texture2D inAccumColor;
[[vk::binding(5, 0)]] Texture2D inAccumMoments;
[[vk::binding(6, 0)]] RWTexture2D<float4> imgFiltered;
[[vk::binding(SSR_CAMERA_BIND, 0)]] ConstantBuffer<Camera> kCam;

static const float2 kPoissonDisk[8] = {
    float2( 0.9284, 0.0000), float2( 0.6564, 0.6564),
    float2( 0.0000, 0.9284), float2(-0.6564, 0.6564),
    float2(-0.9284, 0.0000), float2(-0.6564,-0.6564),
    float2( 0.0000,-0.9284), float2( 0.6564,-0.6564),
};

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 workPos = int2(dispatchThreadID.xy);
    int2 texSize = int2(kSSR.screenSize);
    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
        return;

    float2 uv = (float2(workPos) + 0.5) * kSSR.invScreenSize;
    float centerDepth = gbufDepth.Load(int3(workPos, 0)).r;
    float4 centerColor = inAccumColor.Load(int3(workPos, 0));

    if (centerDepth <= 0.0 || !kSSR.bSpatialEnabled)
    {
        imgFiltered[workPos] = centerColor;
        return;
    }

    float3 centerN = SSR_OctDecodeNormal(gbufNormal.Load(int3(workPos, 0)).rg);
    float3 centerWS = SSR_WorldPosFromDepth(uv, centerDepth, kCam.inverseProjection, kCam.inverseView);
    float centerViewZ = SSR_ViewZ(centerWS, kCam.view);
    float roughness = max(gbufPBR.Load(int3(workPos, 0)).r, 0.02);

    float4 moments = inAccumMoments.Load(int3(workPos, 0));
    float variance = max(moments.g - moments.r * moments.r, 0.0);
    float varianceScale = saturate(sqrt(variance) * kSSR.varianceClampGamma);

    float radiusPx = kSSR.spatialRadiusPx * lerp(0.35, 1.0, roughness) * lerp(0.5, 1.5, varianceScale);
    radiusPx = max(radiusPx, 1.0);

    float rotAngle = BlueNoiseErrorDistrib(uint(workPos.x), uint(workPos.y), uint(kSSR.frameIndex), 3u) * 6.2831853;
    float2x2 rot = float2x2(cos(rotAngle), -sin(rotAngle), sin(rotAngle), cos(rotAngle));

    float3 sumColor = centerColor.rgb;
    float sumConfidence = centerColor.a;
    float sumWeight = 1.0;

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float2 offsetPx = mul(rot, kPoissonDisk[i]) * radiusPx;
        int2 samplePos = clamp(workPos + int2(offsetPx), int2(0, 0), texSize - 1);

        float sampleDepth = gbufDepth.Load(int3(samplePos, 0)).r;
        if (sampleDepth <= 0.0)
            continue;

        float2 sampleUV = (float2(samplePos) + 0.5) * kSSR.invScreenSize;
        float3 sampleN = SSR_OctDecodeNormal(gbufNormal.Load(int3(samplePos, 0)).rg);
        float3 sampleWS = SSR_WorldPosFromDepth(sampleUV, sampleDepth, kCam.inverseProjection, kCam.inverseView);
        float sampleViewZ = SSR_ViewZ(sampleWS, kCam.view);

        float normalWeight = pow(saturate(dot(centerN, sampleN)), 32.0);
        float depthWeight = exp(-abs(sampleViewZ - centerViewZ) / max(centerViewZ * 0.05, 1e-3));
        float weight = normalWeight * depthWeight;

        if (weight < 1e-4)
            continue;

        float4 sampleColor = inAccumColor.Load(int3(samplePos, 0));
        sumColor += sampleColor.rgb * weight;
        sumConfidence += sampleColor.a * weight;
        sumWeight += weight;
    }

    float3 filtered = sumColor / max(sumWeight, 1e-4);
    float filteredConfidence = sumConfidence / max(sumWeight, 1e-4);

    if (anyBadFloat(filtered))
        filtered = centerColor.rgb;

    imgFiltered[workPos] = float4(filtered, filteredConfidence);
}
