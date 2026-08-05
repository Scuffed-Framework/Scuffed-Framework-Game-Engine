#include "Atmosphere/Atmosphere.si"

[[vk::binding(0, 0)]]
ConstantBuffer<AtmoUBO> u;

[[vk::binding(1, 0)]]
RWTexture3D<float4> aerialPerspColorRGBTransR;

[[vk::binding(2, 0)]]
RWTexture3D<float4> aerialPerspTransGB;

[[vk::binding(3, 0)]]
RWTexture2D<float> aerialPerspRange;

// Combined sampler+texture using Sampler2D
[[vk::binding(4, 0)]]
Sampler2D transmittanceLUT;

[[vk::binding(5, 0)]]
Sampler2D multiScatterLUT;

// Reconstruct a world-space ray direction from NDC (x,y) and the UBO matrices.
float3 ndcToRayDir(float2 ndc)
{
    float4 vp = mul(u.invProj, float4(ndc, 1.0, 1.0));
    return normalize(mul(u.invView, float4(vp.xyz / vp.w, 0.0)).xyz);
}

float sliceDepth(float p, float maxDist)
{
    return p * p * maxDist;
}

[numthreads(8, 8, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int3 lutSize;
    aerialPerspColorRGBTransR.GetDimensions(lutSize.x, lutSize.y, lutSize.z);
    int2 coord2 = int2(globalThreadID.xy);
    if (any(coord2 >= lutSize.xy)) return;

    int sliceCount = lutSize.z;
    float2 uv = (float2(coord2) + 0.5) / float2(lutSize.xy);
    float2 ndc = uv * 2.0 - 1.0;

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

    float3 rd = ndcToRayDir(ndc);

    float2 atmoHit = raySphereIntersect(viewPos, rd, Rtop);
    float2 gndHit = raySphereIntersect(viewPos, rd, Rbot);

    // Ray misses the atmosphere entirely, fill with no-op and bail.
    if (atmoHit.y < 0.0)
    {
        aerialPerspRange[coord2] = -1.0;
        for (int z = 0; z < sliceCount; z++)
        {
            int3 c = int3(coord2, z);
            aerialPerspColorRGBTransR[c] = float4(0.0, 0.0, 0.0, 1.0);
            aerialPerspTransGB[c] = float4(1.0, 1.0, 0.0, 0.0);
        }
        return;
    }

    float startDist = max(atmoHit.x, 0.0);
    float endDist = atmoHit.y;

    // Clamp to ground if the ray hits the planet surface.
    if (gndHit.x > 0.0 && gndHit.x < gndHit.y)
        endDist = min(endDist, gndHit.x);

    float distToTravel = max(endDist - startDist, 0.0);

    // Store range for compositor depth → slice mapping.
    aerialPerspRange[coord2] = distToTravel;
    float mu = dot(rd, sunDir);
    float mumu = mu * mu;
    float gg = G * G;
    float phaseR = 3.0 / (16.0 * kPI) * (1.0 + mumu);
    float phaseM = 3.0 / (8.0 * kPI)
                    * ((1.0 - gg) * (mumu + 1.0))
                    / (pow(1.0 + gg - 2.0 * mu * G, 1.5) * (2.0 + gg));

    float camHeight = length(viewPos);
    bool nonLinear = (camHeight < Rtop);

    // State accumulated from the camera outward:
    float3 totalScatter = float3(0.0);
    float3 totalTransmit = float3(1.0);

    float distanceTravelled = 0.0;

    for (int i = 0; i < sliceCount; i++)
    {
        float sliceFrac = float(i + 1) / float(sliceCount);
        float targetDist;
        if (nonLinear)
            targetDist = sliceDepth(sliceFrac, distToTravel);
        else
            targetDist = sliceFrac * distToTravel;

        float stepSize = targetDist - distanceTravelled;

        float tMid = startDist + distanceTravelled + stepSize * 0.5;
        float3 posI = viewPos + rd * tMid;
        float rI = length(posI);
        float altI = max(rI - Rbot, 0.0);

        float3 sigma_s = float3(0.0);
        float3 sigma_t = float3(0.0);

        float rhoRay = exp(-altI / HEIGHT_RAY);
        float rhoMie = exp(-altI / HEIGHT_MIE);
        float ozDen = (HEIGHT_ABSORPTION - altI) / ABSORPTION_FALLOFF;
        float rhoOz = (1.0 / (ozDen * ozDen + 1.0)) * rhoRay;

        sigma_s = RAY_BETA * rhoRay + MIE_BETA * rhoMie;
        sigma_t = sigma_s + ABSORPTION_BETA * rhoOz;

        float3 T_step = exp(-sigma_t * stepSize);
        float3 weight = (float3(1.0) - T_step) / max(sigma_t, float3(1e-7));

        float cosSunI = dot(posI / rI, sunDir);
        
        // Calculate UV coordinates for LUT sampling
        float normalizedHeight = (rI - Rbot) / (Rtop - Rbot);
        float2 lutUV = float2(
            saturate(cosSunI * 0.5 + 0.5),
            saturate(normalizedHeight)
        );
        
        float3 sunTrans = transmittanceLUT.SampleLevel(lutUV, 0).r;
        float3 ms = multiScatterLUT.SampleLevel(lutUV, 0).rgb;

        float3 singleScatter = totalTransmit * weight * sunTrans
                            * (phaseR * RAY_BETA * rhoRay
                            + phaseM * MIE_BETA * rhoMie);

        float3 multiS = totalTransmit * weight * ms * sigma_s;

        totalScatter += (singleScatter + multiS) * sunI;
        totalTransmit *= T_step;

        distanceTravelled = targetDist;

        int3 sliceCoord = int3(coord2, i);
        aerialPerspColorRGBTransR[sliceCoord] = float4(totalScatter, totalTransmit.r);
        aerialPerspTransGB[sliceCoord] = float4(totalTransmit.gb, 0.0, 0.0);
    }
}