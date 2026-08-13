// RayGen.shader — Probed Stochastic SSR, stage 1/5.
//
// Per pixel: decode the GBuffer, build a stable per-pixel/per-frame
// low-discrepancy sample, importance-sample a microfacet normal from the
// surface's GGX lobe (roughness-aware), and reflect the view direction
// about it. Writes out a direction + PDF only — the actual screen-space
// trace happens in Trace.shader, kept as a separate dispatch so either
// stage can be inspected/disabled independently (see SSRParams.debugView).
//
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0   ConstantBuffer SSRParams
//   binding 1   Texture2D      gbufDepth
//   binding 2   Texture2D      gbufNormal
//   binding 3   Texture2D      gbufPBR
//   binding 4   RWTexture2D<float4> imgRayDir   (rgb=dir WS, a=NdotH)
//   binding 5   RWTexture2D<float4> imgRayData  (r=roughnessA, g=metallic, b=skyMask, a=pdf)
//   binding 31  ConstantBuffer Camera (shared, Common/Camera.si)
#include "SSR/SSRCommon.si"

[[vk::binding(0, 0)]] ConstantBuffer<SSRParams> kSSR;
[[vk::binding(1, 0)]] Texture2D gbufDepth;
[[vk::binding(2, 0)]] Texture2D gbufNormal;
[[vk::binding(3, 0)]] Texture2D gbufPBR;
[[vk::binding(4, 0)]] RWTexture2D<float4> imgRayDir;
[[vk::binding(5, 0)]] RWTexture2D<float4> imgRayData;
[[vk::binding(SSR_CAMERA_BIND, 0)]] ConstantBuffer<Camera> kCam;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 workPos = int2(dispatchThreadID.xy);
    int2 texSize = int2(kSSR.screenSize);
    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
        return;

    float2 uv = (float2(workPos) + 0.5) * kSSR.invScreenSize;

    float depth = gbufDepth.Load(int3(workPos, 0)).r;

    // Background (cleared to 0 in reversed-Z) : nothing to reflect off of,
    // skip the lobe sample entirely and flag it for Trace/Composite.
    if (depth <= 0.0)
    {
        imgRayDir[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        imgRayData[workPos] = float4(0.0, 0.0, 1.0, 0.0);
        return;
    }

    float4 pbr = gbufPBR.Load(int3(workPos, 0));
    float roughness = max(pbr.r, 0.02);
    float metallic = pbr.g;

    // Roughness cutoff : past this, the specular lobe is broad enough that
    // screen-space information contributes little over the probe fallback,
    // so skip the trace and let Composite fall back to ambient directly.
    if (roughness > kSSR.maxRoughness)
    {
        imgRayDir[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        imgRayData[workPos] = float4(roughness, metallic, 1.0, 0.0);
        return;
    }

    float3 N = SSR_OctDecodeNormal(gbufNormal.Load(int3(workPos, 0)).rg);
    float3 worldPos = SSR_WorldPosFromDepth(uv, depth, kCam.inverseProjection, kCam.inverseView);
    float3 V = normalize(kCam.cameraPosition.xyz - worldPos);

    // Stable per-pixel, per-frame sample: Owen-scrambled Sobol' (dimensions
    // 0/1), rotated across frames via frameIndex as the sample index so the
    // lobe is re-sampled with a *different* low-discrepancy point each
    // frame rather than repeating the same direction — this is what makes
    // the result suitable for temporal accumulation instead of just being
    // static per-pixel dither.
    float2 xi = float2(
        BlueNoiseErrorDistrib(uint(workPos.x), uint(workPos.y), uint(kSSR.frameIndex), 0u),
        BlueNoiseErrorDistrib(uint(workPos.x), uint(workPos.y), uint(kSSR.frameIndex), 1u));

    float roughnessA = SSR_RoughnessToAlpha(roughness);
    float NdotH;
    float3 R = SSR_SampleReflectionDir(xi, N, V, roughnessA, NdotH);

    // Degenerate sample (lobe pointed into the surface) : fall back to the
    // mirror direction so we never trace a ray that immediately re-enters
    // geometry.
    if (dot(R, N) <= 0.0)
        R = reflect(-V, N);

    float3 Hs = normalize(V + R);
    float HdotV = saturate(dot(Hs, V));
    float pdf = SSR_GGXReflectionPdf(NdotH, HdotV, roughnessA * roughnessA);

    imgRayDir[workPos] = float4(R, NdotH);
    imgRayData[workPos] = float4(roughnessA, metallic, 0.0, pdf);
}
