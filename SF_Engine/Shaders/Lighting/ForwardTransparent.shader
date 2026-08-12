// ForwardTransparent.slang
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0  ConstantBuffer  FrameData
//   binding 1  StructuredBuffer<Light>
//   binding 2  StructuredBuffer<ClusterList>
//   binding 3  StructuredBuffer<uint> lightIndices
//   binding 4  Texture2D sceneHDR
//   binding 5  Texture2D sceneDepth
//   binding 6  SamplerState linearSampler
// Push constants: fmodel float4x4, baseColor float4,
//                 roughness, metallic, ior, refractionStrength
#include "Common/Math.si"
struct FrameData
{
    float4x4 view;        float4x4 proj;       float4x4 viewProj;
    float4x4 invView;     float4x4 invProj;    float4x4 invViewProj;
    float4   cameraPos;   float4   cameraDir;
    float2   screenSize;  float2   invScreenSize;
    float    nearPlane;   float    farPlane;   float time; float deltaTime;
    uint     lightCount;  uint     frameIndex; float2 _pad;
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameData> frame;

struct Light
{
    float3 position; float radius; float3 color; float intensity;
    float3 direction; float innerCone; float outerCone; uint type; float castShadow; float _pad;
};
[[vk::binding(1, 0)]] StructuredBuffer<Light> lights;

struct ClusterList { uint offset; uint count; };
[[vk::binding(2, 0)]] StructuredBuffer<ClusterList> lists;
[[vk::binding(3, 0)]] StructuredBuffer<uint> indices;

[[vk::binding(4, 0)]] Texture2D sceneHDR;
[[vk::binding(5, 0)]] Texture2D sceneDepth;
[[vk::binding(6, 0)]] SamplerState linearSampler;

struct PC
{
    float4x4 fmodel;
    float4   baseColor;
    float roughness; float metallic; float ior; float refractionStrength;
};
[[vk::push_constant]] ConstantBuffer<PC> push;

// ---------------------------------------------------------------------
// Vertex stage
// ---------------------------------------------------------------------

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
    float3x3 tbn          : TBN;
    float4   clipPos      : CLIPPOS;
};

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;

    float4 wp4 = mul(push.fmodel, float4(input.position, 1.0));
    output.worldPos = wp4.xyz;
    output.uv = input.texCoord;

    float3x3 normalMat = transpose(inverse3x3((float3x3)push.fmodel));
    float3 N = normalize(mul(normalMat, input.normal));
    float3 T = normalize(mul(normalMat, input.tangent));
    T = normalize(T - dot(T, N) * N);

    output.tbn = float3x3(T, cross(N, T), N);
    output.svPosition = mul(frame.viewProj, wp4);
    output.clipPos = output.svPosition;
    return output;
}

// ---------------------------------------------------------------------
// Fragment stage
// ---------------------------------------------------------------------

#define PI        3.14159265359
#define CLUSTER_X 16
#define CLUSTER_Y 9
#define CLUSTER_Z 24

float distGGX(float NdH, float a)
{
    float a2 = a * a; // Correct: roughness squared
    float d = NdH * NdH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
float geomSGGX(float NdX, float a)
{
    float k = (a + 1.0) * (a + 1.0) / 8.0; return NdX / (NdX * (1.0 - k) + k);
}
float3 fresnel(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
uint clusterIdx(float2 fragCoord, float3 worldPos)
{
    uint2 tile = uint2(fragCoord / (frame.screenSize / float2(CLUSTER_X, CLUSTER_Y)));
    float vz = -(mul(frame.view, float4(worldPos, 1.0))).z;
    uint sl = uint(max(0.0, log(vz / frame.nearPlane) / log(frame.farPlane / frame.nearPlane) * float(CLUSTER_Z)));
    return tile.x + tile.y * CLUSTER_X + min(sl, uint(CLUSTER_Z - 1)) * CLUSTER_X * CLUSTER_Y;
}

struct FSOutput
{
    float4 color : SV_Target;
};

[shader("fragment")]
FSOutput fragmentMain(VSOutput input)
{
    FSOutput output;

    float3 N = normalize(mul(float3(0.0, 0.0, 1.0), input.tbn));
    float3 V = normalize(frame.cameraPos.xyz - input.worldPos);
    float r0 = (push.ior - 1.0) / (push.ior + 1.0); r0 *= r0;
    float3 F0 = lerp(float3(r0, r0, r0), push.baseColor.rgb, push.metallic);
    float fresnelTerm = fresnel(max(dot(N, V), 0.0), F0).r;

    // Screen-space refraction
    float2 scrUV     = (input.clipPos.xy / input.clipPos.w) * 0.5 + 0.5;
    float2 refractUV = clamp(scrUV + N.xy * push.refractionStrength * (1.0 - fresnelTerm),
                              float2(0.001, 0.001), float2(0.999, 0.999));
    float3 behind     = sceneHDR.Sample(linearSampler, refractUV).rgb;

    // Clustered specular highlights
    float3 spec = float3(0.0, 0.0, 0.0);
    uint cidx = clusterIdx(input.svPosition.xy, input.worldPos);
    for (uint i = 0u; i < lists[cidx].count; i++)
    {
        Light l = lights[indices[lists[cidx].offset + i]];
        float3 L = (l.type == 2u) ? normalize(l.direction) : normalize(l.position - input.worldPos);
        float3 H = normalize(V + L);
        float NdL = max(dot(N, L), 0.0); if (NdL == 0.0) continue;
        float NdH = max(dot(N, H), 0.0), NdV = max(dot(N, V), 0.0), HdV = max(dot(H, V), 0.0);
        float D = distGGX(NdH, push.roughness), G = geomSGGX(NdV, push.roughness) * geomSGGX(NdL, push.roughness);
        float3 F = fresnel(HdV, F0);
        float atten = 1.0;
        if (l.type != 2u)
        {
            float dist = length(l.position - input.worldPos), t = dist / l.radius;
            float w = max(0.0, 1.0 - t * t * t * t); w *= w; atten = w / max(dist * dist, 1e-4);
        }
        spec += (D * G * F) / (4.0 * NdV * NdL + 1e-3) * l.color * l.intensity * atten * NdL;
    }

    float3 color = lerp(behind, push.baseColor.rgb, push.baseColor.a * 0.3) + spec;
    float alpha = clamp(fresnelTerm + push.baseColor.a * 0.5, 0.0, 1.0);
    output.color = float4(color * alpha, alpha); // premultiplied
    return output;
}
