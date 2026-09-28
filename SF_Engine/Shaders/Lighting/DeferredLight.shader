// DeferredLight.slang
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0  ConstantBuffer  FrameData
//   binding 1  StructuredBuffer<Light>
//   binding 2  StructuredBuffer<ClusterList>
//   binding 3  StructuredBuffer<uint> lightIndices
//   binding 4  Texture2D gbufAlbedo
//   binding 5  Texture2D gbufNormal
//   binding 6  Texture2D gbufPBR
//   binding 7  Texture2D gbufDepth
//   binding 8  SamplerState linearSampler

struct FrameData
{
    float4x4 view;        float4x4 proj;       float4x4 viewProj;
    float4x4 invView;     float4x4 invProj;    float4x4 invViewProj;
    float4   cameraPos;   float4   cameraDir;
    float2   screenSize;  float2   invScreenSize;
    float    nearPlane;   float    farPlane;   float time; float deltaTime;
    uint     lightCount;  uint     frameIndex; float2 _pad;
    float4   sunDirIntensity;    // .xyz = toward-sun unit vector, .w = sun intensity
    float4   ambientSkyColor;    // .rgb = sky colour,    .a = ambientIntensity
    float4   ambientGroundColor; // .rgb = ground colour, .a = unused
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameData> frame;

struct Light
{
    float3 position;  float radius;
    float3 color;     float intensity;
    float3 direction; float innerCone;
    float  outerCone; uint  type; float castShadow; float _pad;
};
[[vk::binding(1, 0)]] StructuredBuffer<Light> lights;

struct ClusterList { uint offset; uint count; };
[[vk::binding(2, 0)]] StructuredBuffer<ClusterList> lists;
[[vk::binding(3, 0)]] StructuredBuffer<uint> indices;

[[vk::binding(4, 0)]] Texture2D gbufAlbedo;
[[vk::binding(5, 0)]] Texture2D gbufNormal;
[[vk::binding(6, 0)]] Texture2D gbufPBR;
[[vk::binding(7, 0)]] Texture2D gbufDepth;
[[vk::binding(8, 0)]] SamplerState linearSampler;


struct VSOutput
{
    float4 svPosition : SV_Position;
    float2 uv          : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexIndex : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((vertexIndex << 1) & 2, vertexIndex & 2);
    output.svPosition = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}


#include "Lighting/BRDF/BRDF.si"

#define CLUSTER_X 16
#define CLUSTER_Y 9
#define CLUSTER_Z 24

float3 octDecode(float2 f)
{
    float3 n = float3(f, 1.0 - abs(f.x) - abs(f.y));
    if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}

float3 worldPosFromDepth(float depth, float2 uv)
{
    float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
    float4 wp  = mul(frame.invViewProj, ndc);
    return wp.xyz / wp.w;
}

uint clusterIdx(float2 fragCoord, float3 wp)
{
    // tile.x/tile.y clamped explicitly: fragCoord can round to exactly frame.screenSize at an
    // edge pixel, or frame.screenSize can momentarily disagree with the actual active render
    // target (the same class of staleness bug SSRPipelinePass had before its resize fix) --
    // either way, an unclamped tile.x/tile.y walking past CLUSTER_X/CLUSTER_Y silently reads a
    // neighbouring or out-of-bounds cluster's light list. The sun (a directional light) is
    // unconditionally present in every VALID cluster's list (see ClusterCull.shader), so a
    // genuinely wrong-but-in-bounds cluster would still light the surface -- a fully black
    // result specifically implies reading past the buffer's actual bounds, which is what this
    // clamp forecloses.
    uint2 tile = min(uint2(fragCoord / (frame.screenSize / float2(CLUSTER_X, CLUSTER_Y))),
                     uint2(CLUSTER_X - 1, CLUSTER_Y - 1));
    float viewZ = -(mul(frame.view, float4(wp, 1.0))).z;
    uint  slice = uint(max(0.0,
        log(viewZ / frame.nearPlane) / log(frame.farPlane / frame.nearPlane) * float(CLUSTER_Z)));
    return tile.x + tile.y * CLUSTER_X + min(slice, uint(CLUSTER_Z - 1)) * CLUSTER_X * CLUSTER_Y;
}

float3 evalLight(Light l, float3 P, float3 N, float3 V,
                  float3 albedo, float rough, float metal, float3 F0)
{
    float3 L; float atten = 1.0;
    if (l.type == 2u)
    {
        L = normalize(-l.direction);
    }
    else
    {
        float3 d = l.position - P; float dist = length(d);
        if (dist >= l.radius) return float3(0.0, 0.0, 0.0);
        L = d / dist;
        float t = dist / l.radius, w = max(0.0, 1.0 - t * t * t * t); w *= w;
        atten = w / max(dist * dist, 1e-4);
        if (l.type == 1u)
        {
            float theta = dot(-L, normalize(l.direction));
            atten *= clamp((theta - l.outerCone) / max(l.innerCone - l.outerCone, 1e-4), 0.0, 1.0);
        }
    }
    float NdL = max(dot(N, L), 0.0); if (NdL == 0.0) return float3(0.0, 0.0, 0.0);
    float NdV = max(dot(N, V), 0.0);

    // Disney/UE4 perceptual-roughness remap: alpha = roughness^2, matching what
    // NormalDistributionGGX/GeometricShadowingMaskingGGXCorrelated (BRDF/GGX.si) expect as
    // roughnessA2 = alpha^2. The old ad-hoc distGGX/geomSGGX used `rough` directly as alpha with
    // no remap, so this reads slightly differently on the same slider value than before.
    // expected when swapping to a properly-conventioned BRDF library, not a regression.
    float roughnessA = max(rough * rough, 0.0009); // avoid a fully-singular mirror lobe
    float roughnessA2 = roughnessA * roughnessA;

    // GeometricShadowingMaskingGGXCorrelated (height-correlated Smith) already folds the
    // 4*NdotV*NdotL denominator AND NdotL itself into its result (see SpecularGGX's own
    // comment). Unlike the old geomSGGX, which needed both applied by the caller. So spec is
    // combined with diff*NdL below rather than inside a shared (diff+spec)*NdL.
    //
    // multiScatterCompensation is left at 1 (disabled): proper multi-scatter energy
    // compensation needs a precomputed LUT this engine doesn't have yet. Known, minor
    // simplification (slightly less energetic at high roughness), not a bug.
    float3 spec = SpecularGGX(V, L, N, F0, NdV, roughnessA2, float3(1.0, 1.0, 1.0));

    float HdotV = saturate(dot(normalize(V + L), V));
    float3 F = FresnelSchlick(HdotV, F0);
    float3 diff = (1.0 - F) * (1.0 - metal) * albedo * INV_PI;

    return (diff * NdL + spec) * l.color * l.intensity * atten;
}

struct FSOutput
{
    float4 color : SV_Target;
};

[shader("fragment")]
FSOutput fragmentMain(VSOutput input)
{
    FSOutput output;

    float depth = gbufDepth.Sample(linearSampler, input.uv).r;
    if (depth <= 0.0) { output.color = float4(0.0, 0.0, 0.0, 0.0); return output; }

    float3 albedo = gbufAlbedo.Sample(linearSampler, input.uv).rgb;
    float3 N      = octDecode(gbufNormal.Sample(linearSampler, input.uv).rg);
    float4 pbr    = gbufPBR.Sample(linearSampler, input.uv);
    float rough = max(pbr.r, 0.04), metal = pbr.g, ao = pbr.b, emis = pbr.a;

    float3 wp = worldPosFromDepth(depth, input.uv);
    float3 V  = normalize(frame.cameraPos.xyz - wp);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metal);

    // Real ambient, replacing a flat 0.03/0.07 grey constant: same two-colour sky/ground model
    // SSR's ProbeFallback uses (frame.ambientSkyColor/ambientGroundColor, sourced from SSR's own
    // tunables in SceneRenderer::RenderScene), so a surface's diffuse ambient and its SSR-
    // reflected sky colour agree instead of ambient being unrelated to what SSR shows.
    //
    // Weighted by (1-F)*(1-metal), mirroring evalLight()'s diffuse term below: energy that goes
    // into Fresnel-driven specular reflection (which SSR supplies via ProbeFallback at grazing
    // angles, where F->1) doesn't also go into diffuse. Without this, near-normal incidence showed
    // almost nothing (F0=0.04, near-black placeholder ambient) while grazing incidence showed
    // nearly full sky brightness via SSR alone, the exact 'flips from black to bright depending
    // on where I am looking' symptom. With it, near-normal incidence gets a real, sky-coloured
    // ambient instead of near-black, and hands off to SSR's reflection smoothly as F rises, so the
    // two no longer read as a discontinuity.
    //
    // Also fades with sun elevation (frame.sunDirIntensity.y), matching Lit.shader's ambientDay/
    // ambientNight blend, deferred-path (gbuffer) objects previously had no day/night ambient
    // response at all, unlike forward-path ones.
    float horizonFade = smoothstep(-0.10, 0.0, frame.sunDirIntensity.y);
    float NdV = max(dot(N, V), 0.0);
    float3 Famb = FresnelSchlickWithRoughness(NdV, F0, rough);
    float3 ambientSky = lerp(frame.ambientGroundColor.rgb, frame.ambientSkyColor.rgb, N.y * 0.5 + 0.5)
                       * frame.ambientSkyColor.a;
    float3 ambientNight = float3(0.001, 0.001, 0.001);
    float3 ambient = lerp(ambientNight, ambientSky, horizonFade);

    float3 Lo = ambient * albedo * (1.0 - Famb) * (1.0 - metal) * ao;

    uint cidx   = clusterIdx(input.svPosition.xy, wp);
    uint offset = lists[cidx].offset, count = lists[cidx].count;
    for (uint i = 0u; i < count; i++)
        Lo += evalLight(lights[indices[offset + i]], wp, N, V, albedo, rough, metal, F0);

    Lo += albedo * emis * 4.0;
    output.color = float4(Lo, 1.0);
    return output;
}
