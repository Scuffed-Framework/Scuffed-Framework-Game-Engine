// Trace.shader — Probed Stochastic SSR, stage 2/5.
//
// Consumes the per-pixel stochastic direction from RayGen.shader and walks
// it across the depth buffer in world space (linear march, jittered start,
// binary-search refinement on hit — see March() below). On a valid
// screen-space hit, samples the already-lit opaque scene color ("hdr",
// read cross-stage — see SSRPipelinePass::PreRender for the one-frame-lag
// note). On a miss (walked off-screen, exceeded step budget, or RayGen
// flagged the pixel as background/too-rough), falls through to an analytic
// probe/ambient fallback so screen-space information that "cannot be
// represented" (occluded, off-screen, behind the camera) still produces a
// plausible reflection instead of a hard black/edge artifact.
//
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0   ConstantBuffer SSRParams
//   binding 1   Texture2D      gbufDepth
//   binding 3   Texture2D      hdrScene        (opaque-lit scene colour)
//   binding 4   Texture2D      inRayDir        (from RayGen, rgb=dir, a=NdotH)
//   binding 5   Texture2D      inRayData       (from RayGen, r=roughnessA g=metallic b=skyMask a=pdf)
//   binding 6   RWTexture2D<float4> imgTraceColor (rgb=radiance, a=confidence)
//   binding 7   RWTexture2D<float4> imgTraceHit   (r=hitMask, g=hitViewDist, b=pdf, a=unused)
//   binding 31  ConstantBuffer Camera (shared)
#include "SSR/SSRCommon.si"
#include "Common/Samplers.si"

[[vk::binding(0, 0)]] ConstantBuffer<SSRParams> kSSR;
[[vk::binding(1, 0)]] Texture2D gbufDepth;
[[vk::binding(3, 0)]] Texture2D hdrScene;
[[vk::binding(4, 0)]] Texture2D inRayDir;
[[vk::binding(5, 0)]] Texture2D inRayData;
[[vk::binding(6, 0)]] RWTexture2D<float4> imgTraceColor;
[[vk::binding(7, 0)]] RWTexture2D<float4> imgTraceHit;
[[vk::binding(SSR_CAMERA_BIND, 0)]] ConstantBuffer<Camera> kCam;

// Analytic two-colour sky/ambient probe. This is the "environment fallback"
// referenced in the architecture doc — a placeholder that keeps SSR fully
// self-contained (no hard dependency on the sfSkies/atmosphere subsystem).
// A richer probe (e.g. the atmosphere SkyView LUT, or a baked reflection
// probe grid) can be swapped in later without touching Trace.shader's
// control flow — only this function needs to change.
float3 ProbeFallback(float3 rayDirWorld)
{
    float t = saturate(rayDirWorld.y * 0.5 + 0.5);
    float3 sky = lerp(kSSR.ambientGroundColor, kSSR.ambientSkyColor, t);
    return sky * kSSR.ambientIntensity;
}

// Linear world-space ray march with a reversed-Z-aware thickness test.
// Returns true and fills hitUV/hitT on a valid hit.
bool March(float3 originWS, float3 dirWS, float viewZ0, float jitter,
           out float2 hitUV, out float hitT)
{
    hitUV = float2(0.0, 0.0);
    hitT = 0.0;

    // Step length scales with distance from the camera : marching in fixed
    // world-space units would under-step distant reflections and over-step
    // close ones. strideScale is an artist knob (screen-space "reach").
    float marchLen = max(viewZ0 * kSSR.strideScale, kSSR.thickness * 4.0);
    float stepLen = marchLen / max(float(kSSR.maxSteps), 1.0);

    float t = stepLen * (0.5 + jitter);
    float prevT = 0.0;

    for (int i = 0; i < kSSR.maxSteps; ++i)
    {
        float3 rayWS = originWS + dirWS * t;
        float3 proj = SSR_WorldToScreenUV(rayWS, kCam.viewProjection);

        if (!SSR_UvInBounds(proj.xy))
            return false;

        float sceneDepth = gbufDepth.SampleLevel(pointClampEdgeSampler, proj.xy, 0).r;
        if (sceneDepth <= 0.0) // background : nothing to hit along this step
        {
            prevT = t;
            t += stepLen;
            continue;
        }

        float3 sceneWS = SSR_WorldPosFromDepth(proj.xy, sceneDepth, kCam.inverseProjection, kCam.inverseView);
        float sceneViewZ = SSR_ViewZ(sceneWS, kCam.view);
        float rayViewZ = SSR_ViewZ(rayWS, kCam.view);

        float diff = rayViewZ - sceneViewZ;
        if (diff > 0.0 && diff < (kSSR.thickness + kSSR.depthBufferThicknessBias))
        {
            // Binary-search refine between [prevT, t] to tighten the hit
            // location before we sample colour from it.
            float lo = prevT, hi = t;
            for (int b = 0; b < kSSR.binarySearchSteps; ++b)
            {
                float mid = 0.5 * (lo + hi);
                float3 midWS = originWS + dirWS * mid;
                float3 midProj = SSR_WorldToScreenUV(midWS, kCam.viewProjection);
                float midDepth = gbufDepth.SampleLevel(pointClampEdgeSampler, midProj.xy, 0).r;
                float3 midSceneWS = SSR_WorldPosFromDepth(midProj.xy, midDepth, kCam.inverseProjection, kCam.inverseView);
                float midDiff = SSR_ViewZ(midWS, kCam.view) - SSR_ViewZ(midSceneWS, kCam.view);
                if (midDiff > 0.0)
                    hi = mid;
                else
                    lo = mid;
            }

            hitT = hi;
            float3 hitWS = originWS + dirWS * hitT;
            hitUV = SSR_WorldToScreenUV(hitWS, kCam.viewProjection).xy;
            return true;
        }

        prevT = t;
        t += stepLen;
    }

    return false;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 workPos = int2(dispatchThreadID.xy);
    int2 texSize = int2(kSSR.screenSize);
    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
        return;

    float4 rayDir4 = inRayDir.Load(int3(workPos, 0));
    float4 rayData = inRayData.Load(int3(workPos, 0));
    bool bSky = rayData.b > 0.5;

    float2 uv = (float2(workPos) + 0.5) * kSSR.invScreenSize;
    float depth = gbufDepth.Load(int3(workPos, 0)).r;

    if (bSky || depth <= 0.0 || dot(rayDir4.xyz, rayDir4.xyz) < 1e-6)
    {
        imgTraceColor[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        imgTraceHit[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float3 worldPos = SSR_WorldPosFromDepth(uv, depth, kCam.inverseProjection, kCam.inverseView);
    float3 rayDirWorld = normalize(rayDir4.xyz);
    float pdf = rayData.a;

    float viewZ0 = SSR_ViewZ(worldPos, kCam.view);
    float jitter = BlueNoiseErrorDistrib(uint(workPos.x), uint(workPos.y), uint(kSSR.frameIndex), 2u);

    float2 hitUV;
    float hitT;
    bool bHit = March(worldPos, rayDirWorld, viewZ0, jitter, hitUV, hitT);

    if (bHit)
    {
        float3 hitColor = hdrScene.SampleLevel(linearClampEdgeSampler, hitUV, 0).rgb;
        if (anyBadFloat(hitColor))
            hitColor = float3(0.0, 0.0, 0.0);

        float edgeFade = SSR_ScreenEdgeFade(hitUV, kSSR.edgeFadeStart);
        float confidence = saturate(edgeFade);

        imgTraceColor[workPos] = float4(hitColor, confidence);
        imgTraceHit[workPos] = float4(1.0, hitT, pdf, 0.0);
    }
    else if (kSSR.bProbeFallbackEnabled != 0)
    {
        float3 probeColor = ProbeFallback(rayDirWorld);
        imgTraceColor[workPos] = float4(probeColor, 0.6);
        imgTraceHit[workPos] = float4(0.0, 0.0, pdf, 0.0);
    }
    else
    {
        imgTraceColor[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        imgTraceHit[workPos] = float4(0.0, 0.0, pdf, 0.0);
    }
}
