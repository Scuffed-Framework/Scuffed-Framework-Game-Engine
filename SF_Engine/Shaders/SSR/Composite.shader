// Composite.shader — Probed Stochastic SSR, stage 5/5 (graphics).
//
// IMPORTANT — this used to be a compute shader that read-modify-wrote "hdr"
// directly via RWTexture2D from PreRender(), like Clouds/Composite.shader
// does. That doesn't work here: every attachment in this engine's
// renderpasses is created with loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR
// (RenderPass.cpp), and PreRender for an entire render stage runs — for
// every subpass in that stage — before that stage's renderpass begins. So
// a PreRender compute write to "hdr" gets unconditionally cleared to black
// the instant the renderpass starts, before any subpass (including the one
// that wrote it) ever gets to read it back. This is very likely why
// CloudPipelinePass, which used the same read-modify-write-in-PreRender
// approach for the same attachment, is disabled as broken.
//
// The fix: SSR's compute stages (RayGen/Trace/TemporalAccumulate/
// SpatialFilter — see SSRPipelinePass::PreRender) still run in PreRender,
// but they only ever touch SSR's own private images, never "hdr". This
// shader is the one piece that actually touches "hdr", and it does so as
// a normal graphics draw inside an actual subpass (SSRPipelinePass::Render,
// registered as its own subpass between deferred-light and forward-
// transparent — see SceneRenderer.hpp) — so it draws into "hdr" using the
// same renderpass-managed attachment mechanism every other lighting draw
// uses, and blends additively via the pipeline's fixed-function blend
// state (RenderPipeline::CreateAttributes: finalRGB = srcRGB + dstRGB*(1-srcA),
// so outputting alpha=0 here gives pure additive blending on top of
// whatever deferred lighting already wrote).
//
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0   ConstantBuffer SSRParams
//   binding 1   Texture2D      gbufDepth   (DEPTH_STENCIL_READ_ONLY_OPTIMAL)
//   binding 2   Texture2D      gbufNormal
//   binding 3   Texture2D      gbufAlbedo
//   binding 4   Texture2D      gbufPBR
//   binding 5   Texture2D      inFiltered  (final SSR result, rgb + a=confidence)
//   binding 6   Texture2D      dbgRayDir   (RayGen output, for SSR_DEBUG_RAYDIR)
//   binding 31  ConstantBuffer Camera (shared)
// Sampling uses the shared sampler set (set=1), same as the compute stages.
#include "SSR/SSRCommon.si"
#include "Common/Samplers.si"

[[vk::binding(0, 0)]] ConstantBuffer<SSRParams> kSSR;
[[vk::binding(1, 0)]] Texture2D gbufDepth;
[[vk::binding(2, 0)]] Texture2D gbufNormal;
[[vk::binding(3, 0)]] Texture2D gbufAlbedo;
[[vk::binding(4, 0)]] Texture2D gbufPBR;
[[vk::binding(5, 0)]] Texture2D inFiltered;
[[vk::binding(6, 0)]] Texture2D dbgRayDir;
[[vk::binding(SSR_CAMERA_BIND, 0)]] ConstantBuffer<Camera> kCam;

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv0 : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv0 = uv;
    return output;
}

[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    int2 workPos = int2(input.position.xy);
    float depth = gbufDepth.Load(int3(workPos, 0)).r;

    // Background : contribute nothing (alpha=0, rgb=0 is a no-op under the
    // additive blend below).
    if (depth <= 0.0)
        return float4(0.0, 0.0, 0.0, 0.0);

    if (kSSR.debugView != SSR_DEBUG_NONE)
    {
        // Debug views replace rather than add : output alpha=1 so the fixed
        // blend (srcRGB*1 + dstRGB*(1-srcA)) fully overrides the lit pixel.
        if (kSSR.debugView == SSR_DEBUG_RAYDIR)
            return float4(dbgRayDir.Load(int3(workPos, 0)).xyz * 0.5 + 0.5, 1.0);

        float3 dbg = float3(0.0, 0.0, 0.0);
        if (kSSR.debugView == SSR_DEBUG_SPATIAL || kSSR.debugView == SSR_DEBUG_TEMPORAL || kSSR.debugView == SSR_DEBUG_TRACE_RAW)
            dbg = inFiltered.Load(int3(workPos, 0)).rgb;
        else if (kSSR.debugView == SSR_DEBUG_CONFIDENCE)
            dbg = float3(inFiltered.Load(int3(workPos, 0)).a, 0.0, 0.0);
        return float4(dbg, 1.0);
    }

    float2 uv = input.uv0;
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
    return float4(contribution, 0.0); // alpha=0 -> pure additive under the fixed blend
}
