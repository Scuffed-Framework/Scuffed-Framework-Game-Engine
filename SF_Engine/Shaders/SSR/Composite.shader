// Composite.shader — Probed Stochastic SSR, stage 5/5.
//
// Fresnel-weights the filtered reflection by the surface's roughness and
// specular colour (F0 = lerp(0.04, albedo, metallic), same convention as
// DeferredLight.shader's direct lighting) and additively blends it into
// the "hdr" scene colour target — direct RWTexture2D read-modify-write,
// same pattern Clouds/Composite.shader uses for the same attachment.
// DeferredLight.shader's own ambient term has no specular component, so
// this is a pure addition, not a replace.
//
// kSSR.debugView lets any of the four earlier stages be inspected in
// isolation (written straight to hdr instead of compositing) without
// disabling the pipeline or reaching for a separate visualization pass —
// this is the "individually disabled/debugged" requirement from the
// architecture doc, applied at zero extra dispatch cost.
//
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0   ConstantBuffer SSRParams
//   binding 1   Texture2D      gbufDepth
//   binding 2   Texture2D      gbufNormal
//   binding 3   Texture2D      gbufAlbedo
//   binding 4   Texture2D      gbufPBR
//   binding 5   Texture2D      inFiltered      (final SSR result, rgb + a=confidence)
//   binding 6   Texture2D      dbgRayDir       (RayGen output, for debugView)
//   binding 7   Texture2D      dbgTraceColor   (Trace output, for debugView)
//   binding 8   Texture2D      dbgTemporalColor(TemporalAccumulate output, for debugView)
//   binding 9   RWTexture2D<float4> imgHdrScene
//   binding 31  ConstantBuffer Camera (shared)
#include "SSR/SSRCommon.si"
#include "Common/Samplers.si"

[[vk::binding(0, 0)]] ConstantBuffer<SSRParams> kSSR;
[[vk::binding(1, 0)]] Texture2D gbufDepth;
[[vk::binding(2, 0)]] Texture2D gbufNormal;
[[vk::binding(3, 0)]] Texture2D gbufAlbedo;
[[vk::binding(4, 0)]] Texture2D gbufPBR;
[[vk::binding(5, 0)]] Texture2D inFiltered;
[[vk::binding(6, 0)]] Texture2D dbgRayDir;
[[vk::binding(7, 0)]] Texture2D dbgTraceColor;
[[vk::binding(8, 0)]] Texture2D dbgTemporalColor;
[[vk::binding(9, 0)]] RWTexture2D<float4> imgHdrScene;
[[vk::binding(SSR_CAMERA_BIND, 0)]] ConstantBuffer<Camera> kCam;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 workPos = int2(dispatchThreadID.xy);
    int2 texSize = int2(kSSR.screenSize);
    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
        return;

    float depth = gbufDepth.Load(int3(workPos, 0)).r;
    float3 sceneColor = imgHdrScene[workPos].rgb;

    if (depth <= 0.0)
        return; // background : nothing to reflect onto, leave hdr untouched

    if (kSSR.debugView != SSR_DEBUG_NONE)
    {
        float3 dbg = float3(0.0, 0.0, 0.0);
        if (kSSR.debugView == SSR_DEBUG_RAYDIR)
            dbg = dbgRayDir.Load(int3(workPos, 0)).xyz * 0.5 + 0.5;
        else if (kSSR.debugView == SSR_DEBUG_TRACE_RAW)
            dbg = dbgTraceColor.Load(int3(workPos, 0)).rgb;
        else if (kSSR.debugView == SSR_DEBUG_TEMPORAL)
            dbg = dbgTemporalColor.Load(int3(workPos, 0)).rgb;
        else if (kSSR.debugView == SSR_DEBUG_SPATIAL)
            dbg = inFiltered.Load(int3(workPos, 0)).rgb;
        else if (kSSR.debugView == SSR_DEBUG_CONFIDENCE)
            dbg = float3(inFiltered.Load(int3(workPos, 0)).a, 0.0, 0.0);

        imgHdrScene[workPos] = float4(dbg, 1.0);
        return;
    }

    float2 uv = (float2(workPos) + 0.5) * kSSR.invScreenSize;
    float3 N = SSR_OctDecodeNormal(gbufNormal.Load(int3(workPos, 0)).rg);
    float3 albedo = gbufAlbedo.Load(int3(workPos, 0)).rgb;
    float4 pbr = gbufPBR.Load(int3(workPos, 0));
    float roughness = max(pbr.r, 0.02);
    float metallic = pbr.g;
    float ao = pbr.b;

    float3 worldPos = SSR_WorldPosFromDepth(uv, depth, kCam.inverseProjection, kCam.inverseView);
    float3 V = normalize(kCam.cameraPosition.xyz - worldPos);
    float NdotV = saturate(dot(N, V));

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = FresnelSchlickWithRoughness(NdotV, F0, roughness);

    float4 reflection = inFiltered.Load(int3(workPos, 0));
    if (anyBadFloat(reflection))
        reflection = float4(0.0, 0.0, 0.0, 0.0);

    float3 contribution = reflection.rgb * F * saturate(reflection.a) * ao * kSSR.intensity;
    imgHdrScene[workPos] = float4(sceneColor + contribution, 1.0);
}
