// GBuffer.slang
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0  ConstantBuffer  FrameData
//   binding 1  Texture2D albedoMap
//   binding 2  Texture2D normalMap
//   binding 3  Texture2D pbrMap        (r=roughness, g=metallic, b=AO)
//   binding 4  Texture2D emissiveMap
//   binding 5  SamplerState linearSampler
// Push constants: fmodel float4x4, baseColor float4,
//                 roughnessFactor, metallicFactor, aoFactor, emissiveFactor
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

[[vk::binding(1, 0)]] Texture2D albedoMap;
[[vk::binding(2, 0)]] Texture2D normalMap;
[[vk::binding(3, 0)]] Texture2D pbrMap;
[[vk::binding(4, 0)]] Texture2D emissiveMap;
[[vk::binding(5, 0)]] SamplerState linearSampler;

struct PC
{
    float4x4 fmodel;
    float4   baseColor;
    float roughnessFactor; float metallicFactor; float aoFactor; float emissiveFactor;
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

    output.tbn = float3x3(T, cross(T, N), N);
    output.svPosition = mul(frame.viewProj, wp4);
    return output;
}

// ---------------------------------------------------------------------
// Fragment stage
// ---------------------------------------------------------------------

struct FSOutput
{
    float4 albedo : SV_Target0; // rgb=albedo a=opacity
    float2 normal : SV_Target1; // oct-encoded
    float4 pbr    : SV_Target2; // r=rough g=metal b=ao a=emissive
};

float2 octEncode(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z < 0.0)
    {
        float2 s = sign(n.xy);
        n.xy = (1.0 - abs(n.yx)) * lerp(float2(-1.0, -1.0), float2(1.0, 1.0), step(0.0, s));
    }
    return n.xy;
}

[shader("fragment")]
FSOutput fragmentMain(VSOutput input)
{
    FSOutput output;

    float4 albedo = albedoMap.Sample(linearSampler, input.uv) * push.baseColor;
    if (albedo.a < 0.01) discard;

    float3 tsN    = normalMap.Sample(linearSampler, input.uv).rgb * 2.0 - 1.0;
    float3 worldN = normalize(mul(tsN, input.tbn));
    float4 pbr    = pbrMap.Sample(linearSampler, input.uv);

    output.albedo = float4(albedo.rgb, albedo.a);
    output.normal = octEncode(worldN);
    output.pbr    = float4(pbr.r * push.roughnessFactor,
                            pbr.g * push.metallicFactor,
                            pbr.b * push.aoFactor,
                            emissiveMap.Sample(linearSampler, input.uv).r * push.emissiveFactor);
    return output;
}
