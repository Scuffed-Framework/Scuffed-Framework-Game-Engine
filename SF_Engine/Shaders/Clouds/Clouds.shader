#include "Clouds.si"

[[vk::binding(0, 0)]]
ConstantBuffer<AtmoUBO> atmo;

[[vk::binding(2, 0)]]
Texture2D<float4> blueNoise;

[[vk::binding(3, 0)]]
Texture3D<float4> alligatorNoise;

[[vk::binding(4, 0)]]
Texture2D<float4> transmittanceLUT;

[[vk::binding(5, 0)]]
Texture2D<float4> multiScatterLUT;

[[vk::binding(6, 0)]]
Texture3D<float> baseNoise;

[[vk::binding(7, 0)]]
Texture3D<float> detailNoise;

[[vk::binding(8, 0)]]
Texture2D<float> aerialPerspRange;

[[vk::binding(9, 0)]]
Texture2D<float> sceneDepth;

[[vk::binding(10, 0)]]
Texture2D<float4> sceneColor;

[[vk::binding(11, 0)]]
SamplerState g_sampler;

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput vsmain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    // Fullscreen triangle (same pattern as Atmosphere.shader)
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(uv * 2.0 - 1.0, 1.0, 1.0);
    output.uv = uv;
    return output;
}

float depthToViewDist(float depth, float2 ndc)
{
    float4 clipPos = float4(ndc, depth, 1.0);
    float4 vPos = mul(atmo.invProj, clipPos);
    return length(vPos.xyz / vPos.w);
}


// ---- ray/sphere, returns (near, far) intersection distances ----
float2 RaySphere(float3 center, float radius, float3 ro, float3 rd)
{
    float3 oc = ro - center;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0)
        return float2(1e9, -1.0);
    float s = sqrt(disc);
    return float2(-b - s, -b + s);
}

struct ShellHit
{
    float dstToShell;
    float dstThroughShell;
};

// Intersects the ray with the spherical shell between innerR and outerR,
// handling camera below / inside / above the shell.
ShellHit IntersectCloudShell(float3 ro, float3 rd, float innerR, float outerR, float groundR)
{
    ShellHit hit;
    hit.dstToShell = 0.0;
    hit.dstThroughShell = 0.0;

    float2 outerHit = RaySphere(float3(0.0), outerR, ro, rd);
    if (outerHit.y < 0.0)
        return hit;

    float2 innerHit = RaySphere(float3(0.0), innerR, ro, rd);
    float2 groundHit = RaySphere(float3(0.0), groundR, ro, rd);

    // If solid ground is ahead of the camera along this ray, nothing beyond
    // it (including any "far side" cloud shell) can ever be visible.
    float maxDst = (groundHit.y >= 0.0 && groundHit.x > 0.0) ? groundHit.x : 1e9;

    float h = length(ro);
    float startDst, endDst;

    if (h > outerR)
    {
        if (outerHit.x < 0.0)
            return hit;
        startDst = outerHit.x;
        endDst = outerHit.y;
        if (innerHit.y >= 0.0 && innerHit.x > 0.0)
            endDst = innerHit.x;
    }
    else if (h < innerR)
    {
        if (innerHit.y < 0.0)
            return hit;
        startDst = max(innerHit.y, 0.0);
        endDst = outerHit.y;
    }
    else
    {
        startDst = 0.0;
        endDst = outerHit.y;
        if (innerHit.y >= 0.0 && innerHit.x > 0.0)
            endDst = innerHit.x;
    }

    // Clip both ends against the ground occluder.
    startDst = min(startDst, maxDst);
    endDst = min(endDst, maxDst);

    hit.dstToShell = startDst;
    hit.dstThroughShell = max(0.0, endDst - startDst);
    return hit;
}

float GetCloudDensity(float3 posMetres)
{
    float height = length(posMetres);
    float heightFrac = clamp((height - cloud.cloudBottomRadius) /
                                max(cloud.cloudTopRadius - cloud.cloudBottomRadius, 1.0),
                                0.0, 1.0);

    // Soft falloff at base and top of the layer
    float heightGradient = smoothstep(0.0, 0.2, heightFrac) *
                            (1.0 - smoothstep(0.6, 1.0, heightFrac));
    if (heightGradient <= 0.0)
        return 0.0;

    float3 wind = float3(cloud.time * 20.0, cloud.time * 5.0, cloud.time * 12.0);
    float3 samplePos = (posMetres + wind) * 0.000008; // tile scale, tune per-project

    float baseShape = alligatorNoise.SampleLevel(g_sampler, samplePos, 0.0).r;

    float cov = clamp(cloud.cloudCoverage, 0.0, 1.0);
    float shaped = max(baseShape - (1.0 - cov), 0.0);

    return shaped * heightGradient * cloud.cloudDensityScale;
}

float CloudSunMarch(float3 pos, float3 sunDir)
{
    float stepSize = (cloud.cloudTopRadius - cloud.cloudBottomRadius) / float(LIGHT_STEPS) * 0.5;
    float densitySum = 0.0;
    float3 p = pos;

    for (int i = 0; i < LIGHT_STEPS; i++)
    {
        p += sunDir * stepSize;
        densitySum += GetCloudDensity(p) * stepSize;
    }
    return exp(-densitySum);
}

float4 CloudMarch(float3 origin, float3 dir, float3 sunDir, float dstToShell, float dstThroughShell)
{
    int steps = int(min(cloud.stepCount, float(MAX_STEPS)));
    if (steps <= 0 || dstThroughShell <= 0.0)
        return float4(0.0);

    float stepSize = dstThroughShell / float(steps);

    // Dither the starting offset with blue noise to hide banding
    int2 jitter = int2(
        (cloud.frameIndex * 73) % 128,
        (cloud.frameIndex * 31) % 128
    );

    // Note: In Slang, we need the SV_Position from the fragment shader
    // For now, we'll use a global variable - this will need to be passed from VS
    float2 fragCoord = float2(0.0); // Will be set in fsmain
    int2 pix = (int2(fragCoord.xy) + jitter) % 128;
    float noiseOffset = blueNoise.SampleLevel(g_sampler, float2(pix) / 128.0, 0.0).r;
    float dst = dstToShell + noiseOffset * stepSize;

    float transmittance = 1.0;
    float3 luminance = float3(0.0);

    float sunIntensity = atmo.sunDir.w;
    float3 sunColor = float3(1.0, 0.98, 0.92) * sunIntensity;
    float3 ambientColor = float3(0.4, 0.5, 0.6) * 0.3;

    // Calculate phase for sunlight scattering
    float cosTheta = dot(dir, sunDir);
    // g = 0.6 is a good starting point for forward scattering in clouds
    float phaseVal = HenyeyGreensteinPhaseFunction(cosTheta, 0.6);

    for (int i = 0; i < steps; i++)
    {
        float3 pos = origin + dir * dst;
        float density = GetCloudDensity(pos);

        if (density > 0.0)
        {
            float sunTransmittance = CloudSunMarch(pos, sunDir);
            
            // 1. Apply Phase Function to direct sunlight
            float3 directLight = sunColor * sunTransmittance * phaseVal;
            
            // 2. Attenuate ambient light!
            // The deeper we are (lower transmittance), the darker the ambient light should be.
            float3 ambientLight = ambientColor * (0.2 + 0.8 * sunTransmittance);

            // Combine and multiply by density and step size
            float3 lightEnergy = (directLight + ambientLight) * density * stepSize;

            luminance += lightEnergy * transmittance;
            transmittance *= exp(-density * stepSize);

            if (transmittance < 0.01)
                break;
        }

        dst += stepSize;
    }

    return float4(luminance, 1.0 - transmittance);
}

[shader("fragment")]
float4 fsmain(VSOutput input) : SV_Target
{
    float2 uv = input.uv;
    float2 ndc = uv * 2.0 - 1.0;

    float4 clip = float4(ndc, 1.0, 1.0);
    float4 viewPos = mul(atmo.invProj, clip);
    viewPos /= viewPos.w;
    float3 rayDir = normalize(mul(atmo.invView, float4(viewPos.xyz, 0.0)).xyz);
    
    float3 rayOrigin = atmo.cameraPos.xyz;
    float3 sunDir = atmo.sunDir.xyz;

    float depth = sceneDepth.SampleLevel(g_sampler, uv, 0.0).r;
    float sceneDist = (depth > 0.0) ? depthToViewDist(depth, ndc) : 1e9;

    ShellHit hit = IntersectCloudShell(rayOrigin, rayDir, 
                                        cloud.cloudBottomRadius, 
                                        cloud.cloudTopRadius, 
                                        atmo.bottomRadius);

    hit.dstThroughShell = max(0.0, min(hit.dstToShell + hit.dstThroughShell, sceneDist) - hit.dstToShell);

    if (hit.dstThroughShell <= 0.0)
    {
        return float4(0.0);
    }

    float4 cloudResult = CloudMarch(rayOrigin, rayDir, sunDir, hit.dstToShell, hit.dstThroughShell);

    // 1. Exposure Control (Tweak this value! Try 0.1, 0.01, etc.)
    float exposure = 1.0;
    float3 exposedColor = cloudResult.rgb * exposure;

    // 2. Simple Reinhard Tonemapping (compresses high values down gracefully)
    float3 finalColor = exposedColor / (1.0 + exposedColor);
    
    return float4(finalColor, cloudResult.a);
}