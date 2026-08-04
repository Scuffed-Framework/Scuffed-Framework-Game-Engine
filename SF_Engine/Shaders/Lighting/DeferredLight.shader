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

// ---------------------------------------------------------------------
// Vertex stage : fullscreen triangle, no vertex buffer
// ---------------------------------------------------------------------

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

// ---------------------------------------------------------------------
// Fragment stage
// ---------------------------------------------------------------------

#define PI        3.14159265359
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

float distGGX(float NdH, float a)
{
    float a2 = a * a * a * a; float d = NdH * NdH * (a2 - 1.0) + 1.0;
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

uint clusterIdx(float2 fragCoord, float3 wp)
{
    uint2 tile  = uint2(fragCoord / (frame.screenSize / float2(CLUSTER_X, CLUSTER_Y)));
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
    float3 H = normalize(V + L);
    float NdH = max(dot(N, H), 0.0), NdV = max(dot(N, V), 0.0), HdV = max(dot(H, V), 0.0);
    float D = distGGX(NdH, rough), G = geomSGGX(NdV, rough) * geomSGGX(NdL, rough);
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

    float depth = gbufDepth.Sample(linearSampler, input.uv).r;
    if (depth >= 1.0) { output.color = float4(0.0, 0.0, 0.0, 1.0); return output; }

    float3 albedo = gbufAlbedo.Sample(linearSampler, input.uv).rgb;
    float3 N      = octDecode(gbufNormal.Sample(linearSampler, input.uv).rg);
    float4 pbr    = gbufPBR.Sample(linearSampler, input.uv);
    float rough = max(pbr.r, 0.04), metal = pbr.g, ao = pbr.b, emis = pbr.a;

    float3 wp = worldPosFromDepth(depth, input.uv);
    float3 V  = normalize(frame.cameraPos.xyz - wp);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metal);

    float3 Lo = lerp(float3(0.03, 0.03, 0.03), float3(0.07, 0.07, 0.07), N.y * 0.5 + 0.5) * albedo * ao;

    uint cidx   = clusterIdx(input.svPosition.xy, wp);
    uint offset = lists[cidx].offset, count = lists[cidx].count;
    for (uint i = 0u; i < count; i++)
        Lo += evalLight(lights[indices[offset + i]], wp, N, V, albedo, rough, metal, F0);

    Lo += albedo * emis * 4.0;
    output.color = float4(Lo, 1.0);
    return output;
}
