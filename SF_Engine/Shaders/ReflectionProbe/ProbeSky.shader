// ProbeSky.shader : captures the sky into mip 0 of the reflection probe cubemap (all 6 faces).
//
// With an atmosphere present the radiance comes from the SkyView LUT -- the exact function and
// tonemap Atmosphere.shader uses for sky pixels (sampleSkyView, then 1 - exp(-x)), minus the sun
// disc (the sun's own reflection is the direct-lighting specular's job, and a 1-texel-bright disc
// in a low-res probe would just alias). Without an atmosphere it falls back to the analytic
// two-colour sky/ground gradient from SSR's ambient tunables, so there is always *something*
// sensible in the cube.
//
// Resource layout (set=0):
//   binding 0   ConstantBuffer AtmoUBO   (copy of the atmosphere pass's frame data)
//   binding 1   ConstantBuffer ProbeParams
//   binding 2   Sampler2D      skyViewLUT
//   binding 3   RWTexture2DArray<float4> probeOut   (mip 0, 6 layers)
#include "Atmosphere/Atmosphere.si"
#include "ReflectionProbe/ReflectionProbe.si"

struct ProbeParams
{
    float4 skyColor;    // rgb = analytic sky colour, a = intensity
    float4 groundColor; // rgb = analytic ground colour
    int useAtmosphere;
    int pad0;
    int pad1;
    int pad2;
};

[[vk::binding(0, 0)]] ConstantBuffer<AtmoUBO> atmo;
[[vk::binding(1, 0)]] ConstantBuffer<ProbeParams> probeParams;
[[vk::binding(2, 0)]] Sampler2D skyViewLUT;
[[vk::binding(3, 0)]] RWTexture2DArray<float4> probeOut;

// Fraction of the horizon sky radiance the ground reflects back (rough diffuse albedo of an average
// lit floor). Placeholder for real captured ground radiance.
static const float kGroundReflectance = 0.45;

// Sky radiance for a direction, with the same camera-height guards and tonemap Atmosphere.shader
// applies to sky pixels.
float3 SkyColorAt(float3 rd)
{
    float3 sunDir = normalize(atmo.sunDir.xyz);
    float Rbot = atmo.bottomRadius;

    float3 viewPos = atmo.cameraPos.xyz;
    float vpLen = length(viewPos);
    if (vpLen < 1.0)
        viewPos = float3(0.0, Rbot + 1.0, 0.0);
    else if (vpLen < Rbot + 1.0)
        viewPos = viewPos * ((Rbot + 1.0) / vpLen);
    float camHeight = length(viewPos);

    float3 col = sampleSkyView(rd, viewPos, sunDir, camHeight, Rbot, skyViewLUT);
    return 1.0 - exp(-col);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint w, h, layers;
    probeOut.GetDimensions(w, h, layers);
    if (id.x >= w || id.y >= h || id.z >= 6u)
        return;

    float2 uv = (float2(id.xy) + 0.5) / float2(float(w), float(h));
    float3 rd = ProbeCubeDir(id.z, uv);

    float3 col;
    if (probeParams.useAtmosphere != 0)
    {
        col = SkyColorAt(rd);

        // Below the horizon the SkyView LUT only holds a dark ground-scatter term, but a real floor is
        // an opaque, lit surface: reflecting it should be roughly as bright as the horizon sky times
        // the ground's albedo, not near-black. (Black ground next to a bright sky also made every
        // glossy lobe straddling the horizon extremely noisy.) Blend in below the horizon so there is no seam.
        if (rd.y < 0.0)
        {
            float3 horizDir = float3(rd.x, 0.0, rd.z);
            float horizLen = length(horizDir);
            horizDir = (horizLen > 1e-4) ? horizDir / horizLen : float3(1.0, 0.0, 0.0);
            float3 horizon = SkyColorAt(normalize(float3(horizDir.x, 0.02, horizDir.z)));
            float k = smoothstep(0.0, -0.15, rd.y);
            col = lerp(col, horizon * kGroundReflectance, k);
        }
    }
    else
    {
        float t = saturate(rd.y * 0.5 + 0.5);
        col = lerp(probeParams.groundColor.rgb, probeParams.skyColor.rgb, t) * probeParams.skyColor.a;
    }

    if (any(isnan(col)) || any(isinf(col)))
        col = float3(0.0, 0.0, 0.0);

    probeOut[uint3(id.xy, id.z)] = float4(col, 1.0);
}
