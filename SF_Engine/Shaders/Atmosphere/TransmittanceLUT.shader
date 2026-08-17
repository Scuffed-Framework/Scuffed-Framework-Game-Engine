// TransmittanceLUT.shader
// Baked once at startup. 256×64 RGBA16F.
// Stores T(r, cosZenith) - transmittance from altitude r toward the sun.
//
// UV convention (matches the sampler in Atmosphere.shader):
//   u  → height sqrt-remapped: u=0 at ground, u=1 at atmosphere top
//          height = BOTTOM_RADIUS + u^2 * (TOP_RADIUS - BOTTOM_RADIUS)
//   v  → cosZenith remapped: v=0 → cos=-1 (straight down), v=1 → cos=+1 (straight up)
//
// The sqrt warp packs more texels near the surface where density changes
// fastest, eliminating the banding that appears in the 30-100 km limb band.

#include "Atmosphere/Atmosphere.si"

[[vk::binding(0, 0)]]
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> transmittanceLUT;

void uvToParams(float2 uv, out float height, out float cosZenith) 
{
    float t = uv.x * uv.x;  // inverse of sqrt in heightToU, must match atmosUV()
    height = BOTTOM_RADIUS + t * (TOP_RADIUS - BOTTOM_RADIUS);
    cosZenith = uv.y * 2.0 - 1.0;
}

float distToAtmTop(float h, float cosZ)
{
    float disc = h*h*(cosZ*cosZ - 1.0) + TOP_RADIUS*TOP_RADIUS;
    return max(0.0, -h*cosZ + sqrt(max(0.0, disc)));
}

[numthreads(8, 8, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    int2 size;
    transmittanceLUT.GetDimensions(size.x, size.y);
    int2 coord = int2(threadId.xy);
    if (any(coord >= size)) return;

    // Texel-centre UV
    float2 uv = (float2(coord) + 0.5) / float2(size);
    float h, cosZ;
    uvToParams(uv, h, cosZ);

    float dist = distToAtmTop(h, cosZ);
    float dt = dist / float(STEPS);

    float3 opticalDepth = float3(0.0);
    for (int i = 0; i < STEPS; i++)
    {
        float t = (float(i) + 0.5) * dt;
        // Altitude at sample point along the ray
        float altitude = sqrt(h*h + t*t + 2.0*h*cosZ*t) - BOTTOM_RADIUS;
        altitude = max(altitude, 0.0);

        float rhoRay = exp(-altitude / HEIGHT_RAY);
        float rhoMie = exp(-altitude / HEIGHT_MIE);
        float denom = (HEIGHT_ABSORPTION - altitude) / ABSORPTION_FALLOFF;
        float rhoOz = (1.0 / (denom*denom + 1.0)) * rhoRay;

        opticalDepth += float3(rhoRay, rhoMie, rhoOz) * dt;
    }

    float3 transmittance = exp(
        - RAY_BETA        * opticalDepth.x
        - MIE_BETA        * opticalDepth.y
        - ABSORPTION_BETA * opticalDepth.z);

    transmittanceLUT[coord] = float4(transmittance, 1.0);
}