#include "Atmosphere/Atmosphere.si"

[[vk::binding(0, 0)]]
ConstantBuffer<AtmoUBO> u;

[[vk::binding(1, 0)]]
Texture2D<float4> transmittanceLUT;

[[vk::binding(2, 0)]]
Texture2D<float4> multiScatterLUT;

[[vk::binding(3, 0)]]
Texture2D<float4> skyViewLUT;

[[vk::binding(4, 0)]]
Texture3D<float4> aerialPerspColorRGBTransR;

[[vk::binding(5, 0)]]
Texture3D<float4> aerialPerspTransGB;

[[vk::binding(6, 0)]]
Texture2D<float> aerialPerspRange;

[[vk::binding(7, 0)]]
Texture2D<float4> sceneColor;

[[vk::binding(8, 0)]]
Texture2D<float> sceneDepth;

[[vk::binding(9, 0)]]
SamplerState g_sampler;

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput atmo_vs(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(uv * 2.0 - 1.0, 1.0, 1.0);
    output.uv = uv;
    return output;
}

float3 arbitraryPerp(float3 n) 
{ 
    float3 a = (abs(n.x) < 0.9) ? float3(1, 0, 0) : float3(0, 1, 0); 
    return normalize(cross(n, a)); 
}

float3 sampleSkyView(float3 rd, float3 viewPos, float3 sunDir, float camR, float botRadius)
{
    float3 up = normalize(viewPos);

    float3 sunHoriz = sunDir - dot(sunDir, up) * up;
    float sunHorizLen = length(sunHoriz);
    float3 sunProj = (sunHorizLen > 1e-4)
                        ? (sunHoriz / sunHorizLen)
                        : arbitraryPerp(up);
    float3 perpAxis = cross(up, sunProj);

    float sinTh = clamp(dot(rd, up), -1.0, 1.0);
    float theta = asin(sinTh);
    float v = skyViewEncodeV(theta, camR, botRadius);

    float3 rdH = rd - sinTh * up;
    float rdHLen = length(rdH);
    float u_coord;
    if (rdHLen < 1e-4)
    {
        u_coord = 0.0;
    }
    else
    {
        float3 rdHoriz = rdH / rdHLen;
        float phi = atan2(dot(rdHoriz, perpAxis), dot(rdHoriz, sunProj));
        u_coord = abs(phi) / kPI;
    }

    return skyViewLUT.SampleLevel(g_sampler, float2(u_coord, v), 0.0).rgb;
}

float3 sunDisk(float3 rd, float3 sunDir, float sunIntensity,
                float3 viewPos, float bottomRadius, float topRadius)
{
    const float SUN_ANGULAR_RADIUS = 0.0045;
    float cosAngle = dot(rd, sunDir);
    float diskWeight = smoothstep(
        cos(SUN_ANGULAR_RADIUS * 1.05),
        cos(SUN_ANGULAR_RADIUS * 0.95),
        cosAngle);
    if (diskWeight <= 0.0) return float3(0.0);
    float camR = length(viewPos);
    float cosSun = dot(viewPos / camR, sunDir);
    float3 T = sampleTransmittance(transmittanceLUT, g_sampler, camR, cosSun,
                                    bottomRadius, topRadius);
    return diskWeight * sunIntensity * T;
}

void sampleAerialPerspective(float2 screenUV, float sceneDist,
                                out float3 outScatter, out float3 outTransmit)
{
    float maxDist = max(aerialPerspRange.SampleLevel(g_sampler, screenUV, 0.0).r, 0.001);
    float t = clamp(sqrt(sceneDist / maxDist), 0.0, 1.0);

    float4 ct = aerialPerspColorRGBTransR.SampleLevel(g_sampler, float3(screenUV, t), 0.0);
    float2 tgb = aerialPerspTransGB.SampleLevel(g_sampler, float3(screenUV, t), 0.0).rg;

    outScatter = ct.rgb;
    outTransmit = float3(ct.a, tgb.r, tgb.g);
}

float depthToViewDist(float depth, float2 ndc)
{
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 vPos = mul(u.invProj, clipPos);
    return length(vPos.xyz / vPos.w);
}

[shader("fragment")]
float4 atmo_fs(VSOutput input) : SV_Target
{
    float2 screenUV = input.uv;
    float2 ndc = screenUV * 2.0 - 1.0;

    // Reconstruct world-space ray direction
    float4 vp = mul(u.invProj, float4(ndc, 1.0, 1.0));
    float3 rd = normalize(mul(u.invView, float4(vp.xyz / vp.w, 0.0)).xyz);

    float3 sunDir = normalize(u.sunDir.xyz);
    float sunI = u.sunDir.w;
    float Rbot = u.bottomRadius;
    float Rtop = u.topRadius;

    float3 viewPos = u.cameraPos.xyz;
    float vpLen = length(viewPos);
    if (vpLen < 1.0)
        viewPos = float3(0.0, Rbot + 1.0, 0.0);
    else if (vpLen < Rbot + 1.0)
        viewPos = viewPos * ((Rbot + 1.0) / vpLen);

    float camHeight = length(viewPos);

    // shit way to avoid fixing depth :(
    //float depth = 0.0f;
    // this down here causes a super dull blue atmo :( harass the goat claude to fix it ig
    // probabfly because whatever wrote to it (litmeshpipelinepass) wasn't configured to read and write (neither is this, fuck) the depth for infinite far plane
    // along with the reversed Z buffer.
    float depth = sceneDepth.SampleLevel(g_sampler, screenUV, 0.0).r;
    if (depth > 0.0)
    {
        float3 surface = sceneColor.SampleLevel(g_sampler, screenUV, 0.0).rgb;
        float sceneDist = depthToViewDist(depth, ndc);

        float3 scatter, transmit;
        sampleAerialPerspective(screenUV, sceneDist, scatter, transmit);

        return float4(surface * transmit + scatter, 1.0);
    }

    float2 atmoHit = raySphereIntersect(viewPos, rd, Rtop);

    float camAlt = camHeight - Rbot;
    float atmoThickness = Rtop - Rbot;

    if (camAlt < atmoThickness - 1.0)
    {
        // Inside atmosphere → SkyView LUT
        float3 col = sampleSkyView(rd, viewPos, sunDir, camHeight, Rbot);
        col += sunDisk(rd, sunDir, sunI, viewPos, Rbot, Rtop);
        col = 1.0 - exp(-col);

        float cosSky = dot(normalize(viewPos), rd);
        float3 skyTrans = sampleTransmittance(transmittanceLUT, g_sampler, camHeight,
                                                cosSky, Rbot, Rtop);
        float atmAlpha = 1.0 - dot(skyTrans, float3(0.2126, 0.7152, 0.0722));

        return float4(col, atmAlpha);
    }

    // Above atmosphere → full raymarch
    float maxDist = atmoHit.y;
    float2 gndHit = raySphereIntersect(viewPos, rd, Rbot);
    if (gndHit.x > 0.0 && gndHit.x < gndHit.y)
        maxDist = gndHit.x;

    float3 transmittance;
    float3 col = calculateScattering(
        viewPos, rd, maxDist,
        sunDir, float3(sunI),
        Rbot, Rtop,
        transmittanceLUT, multiScatterLUT, g_sampler,
        transmittance);

    bool groundBlocking = (gndHit.x > 0.0 && gndHit.x < gndHit.y);
    if (!groundBlocking)
        col += sunDisk(rd, sunDir, sunI, viewPos, Rbot, Rtop);

    col = 1.0 - exp(-col);
    float atmAlpha = 1.0 - dot(transmittance, float3(0.2126, 0.7152, 0.0722));
    return float4(col, atmAlpha);
}