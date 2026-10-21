// TemporalAccumulate.shader — Probed Stochastic SSR, stage 3/5.
//
// Reprojects last frame's accumulated reflection using the shared camera's
// prevViewProjection (see Common/Camera.si), neighborhood-clamps the
// history against this frame's freshly traced 3x3 neighbourhood (standard
// TAA-style AABB clip — this is what actually suppresses ghosting on
// disocclusion, since the engine doesn't keep a history normal/depth
// buffer for SSR to cross-check against), and blends. Also accumulates
// luminance moments (mean, mean^2) so SpatialFilter.shader can size its
// kernel from the estimated variance instead of using a fixed radius.
//
// Ping-ponged across kFramesInFlight history slots by SSRPipelinePass —
// see its PreRender() for the slot bookkeeping (same scheme as
// CloudPipelinePass's reconColor_/reconDepth_ ping-pong).
//
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0   ConstantBuffer SSRParams
//   binding 1   Texture2D      gbufDepth        (current frame)
//   binding 3   Texture2D      inTraceColor     (from Trace, rgb, a=confidence)
//   binding 4   Texture2D      inTraceHit       (from Trace, r=hitMask g=hitT b=pdf)
//   binding 5   Texture2D      inHistoryColor   (previous accum, rgb + a=confidence)
//   binding 6   Texture2D      inHistoryMoments (previous moments: r=mean g=mean2 b=historyCount)
//   binding 7   RWTexture2D<float4> imgAccumColor
//   binding 8   RWTexture2D<float4> imgAccumMoments
//   binding 31  ConstantBuffer Camera (shared)
#include "SSR/SSRCommon.si"
#include "Common/Samplers.si"

[[vk::binding(0, 0)]] ConstantBuffer<SSRParams> kSSR;
[[vk::binding(1, 0)]] Texture2D gbufDepth;
[[vk::binding(3, 0)]] Texture2D inTraceColor;
[[vk::binding(4, 0)]] Texture2D inTraceHit;
[[vk::binding(5, 0)]] Texture2D inHistoryColor;
[[vk::binding(6, 0)]] Texture2D inHistoryMoments;
[[vk::binding(7, 0)]] RWTexture2D<float4> imgAccumColor;
[[vk::binding(8, 0)]] RWTexture2D<float4> imgAccumMoments;
[[vk::binding(SSR_CAMERA_BIND, 0)]] ConstantBuffer<Camera> kCam;

float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 workPos = int2(dispatchThreadID.xy);
    int2 texSize = int2(kSSR.screenSize);
    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
        return;

    float4 curColor = inTraceColor.Load(int3(workPos, 0));
    float4 curHit = inTraceHit.Load(int3(workPos, 0));

    if (!kSSR.bTemporalEnabled)
    {
        imgAccumColor[workPos] = curColor;
        imgAccumMoments[workPos] = float4(Luma(curColor.rgb), Luma(curColor.rgb) * Luma(curColor.rgb), 1.0, 0.0);
        return;
    }

    float2 uv = (float2(workPos) + 0.5) * kSSR.invScreenSize;
    float depth = gbufDepth.Load(int3(workPos, 0)).r;

    // Background / no-reflection pixel : nothing to accumulate.
    if (depth <= 0.0)
    {
        imgAccumColor[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        imgAccumMoments[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // 3x3 neighbourhood AABB of this frame's fresh trace, used to clamp
    // (not just blend) the reprojected history so a stale reflection from
    // a now-disoccluded direction gets pulled back toward plausible values
    // instead of ghosting for several frames.
    float3 nMin = curColor.rgb, nMax = curColor.rgb;
    [unroll]
    for (int dy = -1; dy <= 1; ++dy)
    {
        [unroll]
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0) continue;
            int2 samplePos = clamp(workPos + int2(dx, dy), int2(0, 0), texSize - 1);
            float3 c = inTraceColor.Load(int3(samplePos, 0)).rgb;
            nMin = min(nMin, c);
            nMax = max(nMax, c);
        }
    }

    float3 worldPos = SSR_WorldPosFromDepth(uv, depth, kCam.inverseProjection, kCam.inverseView);
    float3 prevUVZ = SSR_WorldToScreenUV(worldPos, kCam.prevViewProjection);
    bool bValidHistory = SSR_UvInBounds(prevUVZ.xy);

    float4 histColor = float4(0.0, 0.0, 0.0, 0.0);
    float4 histMoments = float4(0.0, 0.0, 0.0, 0.0);
    if (bValidHistory)
    {
        histColor = inHistoryColor.SampleLevel(linearClampEdgeSampler, prevUVZ.xy, 0);
        histMoments = inHistoryMoments.SampleLevel(linearClampEdgeSampler, prevUVZ.xy, 0);
    }

    if (anyBadFloat(histColor)) histColor = float4(0.0, 0.0, 0.0, 0.0);
    if (anyBadFloat(histMoments)) histMoments = float4(0.0, 0.0, 0.0, 0.0);

    float historyCount = bValidHistory ? histMoments.b : 0.0;

    float3 clampedHistory = clamp(histColor.rgb, nMin, nMax);

    // More new-frame weight while history is young / invalid, converging
    // toward temporalBlendMin as historyCount grows — same shape as a
    // standard TAA exponential moving average with a bootstrap ramp.
    float alpha = bValidHistory
        ? clamp(1.0 / (historyCount + 1.0), kSSR.temporalBlendMin, kSSR.temporalBlendMax)
        : 1.0;

    // A hit with zero confidence (skipped pixel) shouldn't erase good
    // history outright — decay it gently instead of snapping to black.
    if (curColor.a <= 0.0 && bValidHistory)
    {
        alpha = min(alpha, kSSR.temporalBlendMin);
    }

    float3 outColor = lerp(clampedHistory, curColor.rgb, alpha);
    float outConfidence = lerp(histColor.a, curColor.a, alpha);

    float curLuma = Luma(curColor.rgb);
    float outMean = lerp(histMoments.r, curLuma, alpha);
    float outMean2 = lerp(histMoments.g, curLuma * curLuma, alpha);
    float outCount = min(historyCount + 1.0, 256.0);

    imgAccumColor[workPos] = float4(outColor, outConfidence);
    imgAccumMoments[workPos] = float4(outMean, outMean2, outCount, curHit.r);
}
