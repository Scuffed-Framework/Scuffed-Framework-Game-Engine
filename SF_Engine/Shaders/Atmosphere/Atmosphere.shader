#include "Atmosphere/Atmosphere.si"

// -----------------------------------------------------------------------
// Atmosphere composite — COMPUTE VERSION.
//
// Previously a fullscreen vertex/fragment pass that drew into the
// "swapchain" attachment (relying on hardware alpha blending for the
// sky-vs-background case). Now a single compute kernel that reads and
// writes the "hdr" scene-color image *in place*: it loads whatever the
// opaque pass already wrote there (or the clear colour, for empty sky
// pixels), composites the atmosphere/aerial-perspective result on top,
// and stores the result back into the same texel. No separate
// "sceneColor" input is needed any more — the storage image IS the
// scene colour target.
// -----------------------------------------------------------------------

[[vk::binding(0, 0)]]
ConstantBuffer<AtmoUBO> u;

[[vk::binding(1, 0)]]
Sampler2D transmittanceLUT;

[[vk::binding(2, 0)]]
Sampler2D skyViewLUT;

[[vk::binding(3, 0)]]
Sampler3D aerialPerspColorRGBTransR;

[[vk::binding(4, 0)]]
Sampler3D aerialPerspTransGB;

[[vk::binding(5, 0)]]
Sampler2D aerialPerspRange;

[[vk::binding(6, 0)]]
Sampler2D sceneDepth;

[[vk::binding(7, 0)]]
RWTexture2D<float4> hdrColor;

float3 sunDisk(float3 rd, float3 sunDir, float sunIntensity,
               float3 viewPos, float bottomRadius, float topRadius)
{
    const float SUN_ANGULAR_RADIUS = 0.0045;
    float cosAngle = dot(rd, sunDir);
    float diskWeight = smoothstep(
        cos(SUN_ANGULAR_RADIUS * 1.05),
        cos(SUN_ANGULAR_RADIUS * 0.95),
        cosAngle);
    if (diskWeight <= 0.0)
        return float3(0.0);
    float camR = length(viewPos);
    float cosSun = dot(viewPos / camR, sunDir);
    float3 T = getSpaceTransmittance(transmittanceLUT, viewPos, sunDir, bottomRadius, topRadius);
    return diskWeight * sunIntensity * T;
}

void sampleAerialPerspective(float2 screenUV, float sceneDist,
                             out float3 outScatter, out float3 outTransmit)
{
    float maxDist = max(aerialPerspRange.SampleLevel(screenUV, 0.0).r, 0.001);
    float t = clamp(sqrt(sceneDist / maxDist), 0.0, 1.0);

    float4 ct = aerialPerspColorRGBTransR.SampleLevel(float3(screenUV, t), 0.0);
    float2 tgb = aerialPerspTransGB.SampleLevel(float3(screenUV, t), 0.0).rg;

    outScatter = ct.rgb;
    outTransmit = float3(ct.a, tgb.r, tgb.g);
}

float depthToViewDist(float depth, float2 ndc)
{
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 vPos = mul(u.invProj, clipPos);
    return length(vPos.xyz / vPos.w);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void atmo_cs(uint3 globalThreadID: SV_DispatchThreadID)
{
    uint2 dims;
    hdrColor.GetDimensions(dims.x, dims.y);
    uint2 pixel = globalThreadID.xy;
    if (any(pixel >= dims))
        return;

    float2 screenUV = (float2(pixel) + 0.5) / float2(dims);
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

    float depth = sceneDepth.SampleLevel(screenUV, 0.0).r;
    if (depth > 0.0)
    {
        // Geometry hit: composite aerial perspective directly over whatever
        // the opaque pass already wrote for this texel and store it back.
        float3 surface = hdrColor[pixel].rgb;
        float sceneDist = depthToViewDist(depth, ndc);

        float3 scatter, transmit;
        sampleAerialPerspective(screenUV, sceneDist, scatter, transmit);
        hdrColor[pixel] = float4(surface * transmit + scatter, 1.0);
        return;
    }

    float2 atmoHit = raySphereIntersect(viewPos, rd, Rtop);
    float camAlt = camHeight - Rbot;
    float atmoThickness = Rtop - Rbot;

    float3 col;
    float atmAlpha;
    // Inside atmosphere → SkyView LUT
    col = sampleSkyView(rd, viewPos, sunDir, camHeight, Rbot, skyViewLUT);
    col += sunDisk(rd, sunDir, sunI, viewPos, Rbot, Rtop);
    col = 1.0 - exp(-col);

    float cosSky = dot(normalize(viewPos), rd);
    float3 skyTrans = sampleTransmittance(transmittanceLUT, camHeight,
                                          cosSky, Rbot, Rtop);
    atmAlpha = 1.0 - dot(skyTrans, float3(0.2126, 0.7152, 0.0722));

    hdrColor[pixel] = float4(col, 1.0);
}
