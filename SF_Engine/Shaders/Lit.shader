// Lit.slang : Standard opaque PBR mesh shader (forward, single-pass).
// Use for any opaque mesh that receives clustered lighting.
// Equivalent to Unity's "Lit" / Unreal's default lit material.
//
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0  ConstantBuffer  FrameData
//   binding 1  StructuredBuffer<Light>
//   binding 2  StructuredBuffer<ClusterList>
//   binding 3  StructuredBuffer<uint> lightIndices
//   binding 4  Texture2D albedoMap
//   binding 5  Texture2D normalMap
//   binding 6  Texture2D pbrMap        (r=roughness, g=metallic, b=AO)
//   binding 7  Texture2D emissiveMap
//   binding 8  SamplerState linearSampler
// Push constants: fmodel float4x4, baseColor float4,
//                 roughnessFactor, metallicFactor, aoFactor, emissiveFactor

#include "Common/Math.si"
struct FrameData
{
    float4x4 view;         float4x4 proj;        float4x4 viewProj;
    float4x4 invView;      float4x4 invProj;     float4x4 invViewProj;
    float4   cameraPos;    float4   cameraDir;
    float2   screenSize;   float2   invScreenSize;
    float    nearPlane;    float    farPlane;    float time; float deltaTime;
    uint     lightCount;   uint     frameIndex;  float2 _pad;
    float4   sunDirIntensity; // .xyz = toward-sun unit vector, .w = sun intensity
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameData> frame;

struct Light
{
    float3 position;   float radius;
    float3 color;      float intensity;
    float3 direction;  float innerCone;
    float  outerCone;  uint  type; float castShadow; float _pad;
};

[[vk::binding(1, 0)]] StructuredBuffer<Light> lights;

struct ClusterList { uint offset; uint count; };
[[vk::binding(2, 0)]] StructuredBuffer<ClusterList> lists;
[[vk::binding(3, 0)]] StructuredBuffer<uint> indices;

[[vk::binding(4, 0)]] Texture2D albedoMap;
[[vk::binding(5, 0)]] Texture2D normalMap;
[[vk::binding(6, 0)]] Texture2D pbrMap;
[[vk::binding(7, 0)]] Texture2D emissiveMap;
[[vk::binding(8, 0)]] SamplerState linearSampler;

struct PC
{
    float4x4 fmodel;
    float4   baseColor;
    float roughnessFactor; float metallicFactor; float aoFactor; float emissiveFactor;
};
[[vk::push_constant]] ConstantBuffer<PC> push;

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
    float3 tangent  : TANGENT;
};

struct VSOutput
{
    float4   svPosition : SV_Position;
    float3   worldPos   : WORLDPOS;
    float2   uv          : TEXCOORD0;
    float3x3 tbn          : TBN;   // tangent, bitangent, normal (world space)
};

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;

    float4 wp4 = mul(push.fmodel, float4(input.position, 1.0));
    output.worldPos = wp4.xyz;
    output.uv = input.texCoord;

    // Normal matrix (inverse-transpose) for non-uniform scale correctness
    float3x3 normalMat = transpose(inverse3x3((float3x3)push.fmodel));
    float3 N = normalize(mul(normalMat, input.normal));

    // Tangent transformed by fmodel matrix (NOT normal matrix) then
    // re-orthogonalised against world-space N so the TBN frame is
    // always orthonormal even with non-uniform scale.
    float3 T = normalize(mul((float3x3)push.fmodel, input.tangent));
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T); // right-handed bitangent

    output.tbn = float3x3(T, B, N);
    output.svPosition = mul(frame.viewProj, wp4);
    return output;
}

#define PI        3.14159265359
#define CLUSTER_X 16
#define CLUSTER_Y 9
#define CLUSTER_Z 24

float distGGX(float NdH, float a)
{
    float a2 = a * a * a * a;
    float d = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float geomSGGX(float NdX, float a)
{
    float k = (a + 1.0) * (a + 1.0) / 8.0;
    return NdX / (NdX * (1.0 - k) + k);
}

float3 fresnel(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ACES filmic tonemap (Hill 2018 approximation)
float3 acesFilm(float3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

uint clusterIdx(float2 fragCoord, float3 worldPos)
{
    uint2 tile = uint2(fragCoord / (frame.screenSize / float2(CLUSTER_X, CLUSTER_Y)));
    float vz = -(mul(frame.view, float4(worldPos, 1.0))).z;
    uint sl = uint(max(0.0,
        log(vz / frame.nearPlane) / log(frame.farPlane / frame.nearPlane) * float(CLUSTER_Z)));
    return tile.x + tile.y * CLUSTER_X + min(sl, uint(CLUSTER_Z - 1)) * CLUSTER_X * CLUSTER_Y;
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
        float3 d = l.position - P;
        float dist = length(d);
        if (dist >= l.radius) return float3(0.0, 0.0, 0.0);
        L = d / dist;
        float t = dist / l.radius;
        float w = max(0.0, 1.0 - t * t * t * t);
        w *= w;
        atten = w / max(dist * dist, 1e-4);
        if (l.type == 1u)
        {
            float theta = dot(-L, normalize(l.direction));
            atten *= clamp((theta - l.outerCone) / max(l.innerCone - l.outerCone, 1e-4), 0.0, 1.0);
        }
    }

    float NdL = max(dot(N, L), 0.0);
    if (NdL == 0.0) return float3(0.0, 0.0, 0.0);

    float3 H = normalize(V + L);
    float NdH = max(dot(N, H), 0.0);
    float NdV = max(dot(N, V), 0.0);
    float HdV = max(dot(H, V), 0.0);

    float D = distGGX(NdH, rough);
    float G = geomSGGX(NdV, rough) * geomSGGX(NdL, rough);
    float3 F = fresnel(HdV, F0);

    float3 spec = (D * G * F) / max(4.0 * NdV * NdL, 1e-3);
    float3 diff = (1.0 - F) * (1.0 - metal) * albedo / PI;

    return (diff + spec) * l.color * l.intensity * atten * NdL;
}

struct FSOutput
{
    float4 color : SV_Target;
};

[shader("fragment")]
FSOutput fragmentMain(VSOutput input)
{
    FSOutput output;

    float4 albedoS = albedoMap.Sample(linearSampler, input.uv) * push.baseColor;
    if (albedoS.a < 0.01) discard;

    float3 tsN = normalMap.Sample(linearSampler, input.uv).rgb * 2.0 - 1.0;
    float3 N = normalize(mul(tsN, input.tbn));

    float4 pbr   = pbrMap.Sample(linearSampler, input.uv);
    float rough  = max(pbr.r * push.roughnessFactor, 0.04);
    float metal  = pbr.g * push.metallicFactor;
    float ao     = pbr.b * push.aoFactor;
    float emis   = emissiveMap.Sample(linearSampler, input.uv).r * push.emissiveFactor;

    float3 albedo = albedoS.rgb;
    float3 V      = normalize(frame.cameraPos.xyz - input.worldPos);
    float3 F0     = lerp(float3(0.04, 0.04, 0.04), albedo, metal);

    // Sun visibility: continuously proportional to sun elevation.
    //   sunElevation > 0  -> sun above horizon, full contribution scales with height
    //   sunElevation = 0  -> horizon, soft twilight
    //   sunElevation < 0  -> below horizon, fades through civil twilight to zero
    float sunElevation  = frame.sunDirIntensity.y;
    float horizonFade   = smoothstep(-0.10, 0.0, sunElevation);
    float sunVisibility = max(sunElevation, 0.0) * horizonFade;

    // Ambient: hemisphere term at day, near-black floor at night.
    float3 ambientDay   = lerp(float3(0.03, 0.03, 0.03), float3(0.07, 0.07, 0.07), N.y * 0.5 + 0.5);
    float3 ambientNight = float3(0.001, 0.001, 0.001);
    float3 Lo = lerp(ambientNight, ambientDay, horizonFade) * albedo * ao;

    // Directional lights: scaled by sunVisibility. Point/spot lights are
    // NOT scaled (artificial sources, sun-independent).
    for (uint i = 0u; i < frame.lightCount; i++)
    {
        if (lights[i].type == 2u)
            Lo += evalLight(lights[i], input.worldPos, N, V, albedo, rough, metal, F0) * sunVisibility;
    }

    // Clustered light loop (point + spot only)
    uint cidx   = clusterIdx(input.svPosition.xy, input.worldPos);
    uint offset = lists[cidx].offset;
    uint count  = lists[cidx].count;
    for (uint i = 0u; i < count; i++)
    {
        uint lidx = indices[offset + i];
        if (lights[lidx].type != 2u) // skip directionals already done
            Lo += evalLight(lights[lidx], input.worldPos, N, V, albedo, rough, metal, F0);
    }

    // Emissive
    Lo += albedo * emis * 4.0;

    // Tonemap + gamma
    Lo = acesFilm(Lo);
    Lo = pow(Lo, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    output.color = float4(Lo, 1.0);
    return output;
}
