#include "Atmosphere/Atmosphere.si"

[[vk::binding(0, 0)]]
Sampler2D<float4> transmittanceLUT;

[[vk::binding(1, 0)]]
Sampler2D<float4> multiScatterLUT;

[[vk::binding(2, 0)]]
RWTexture2D<float4> skyViewLUT;

[[vk::binding(3, 0)]]
ConstantBuffer<SkyViewUBO> pc;

[numthreads(8, 8, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int2 coord = int2(globalThreadID.xy);
    int2 size;
    skyViewLUT.GetDimensions(size.x, size.y);
    if (coord.x >= size.x || coord.y >= size.y) return;

    float2 uv = (float2(coord) + 0.5) / float2(size);

    float h = max(length(pc.camPos.xyz), pc.bottomRadius + 1.0);
    float3 viewPos = normalize(pc.camPos.xyz) * h;
    float3 up = normalize(viewPos);

    float3 sunDir = normalize(pc.sunDir.xyz);
    float3 sunHoriz = sunDir - dot(sunDir, up) * up;
    float sunHLen = length(sunHoriz);

    float3 sunProj = (sunHLen > 1e-4) ? (sunHoriz / sunHLen) : arbitraryPerp(up);
    float3 perpAxis = cross(up, sunProj);

    float phi = uv.x * kPI;
    float theta = skyViewDecodeV(uv.y, h, pc.bottomRadius);

    // phi=0 → toward sun horizontally; matches sampleSkyView convention
    float3 rd = cos(theta) * (cos(phi) * sunProj + sin(phi) * perpAxis)
            + sin(theta) * up;

    float2 gndHit = raySphereIntersect(viewPos, rd, pc.bottomRadius);
    float maxDist = 1e12;
    if (gndHit.x > 0.0 && gndHit.x < gndHit.y)
        maxDist = gndHit.x;

    float3 transmittance;
    float3 col = calculateScattering(
        viewPos, rd, maxDist,
        sunDir, float3(pc.sunDir.w),
        pc.bottomRadius, pc.topRadius,
        transmittanceLUT, multiScatterLUT,
        transmittance);

    skyViewLUT[coord] = float4(col, 1.0);
}