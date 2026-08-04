#include "Atmosphere/Atmosphere.si"

const static int SPHERE_SAMPLES = 64; // must equal local_size_z

[[vk::binding(0, 0)]]
Texture2D<float4> transmittanceLUT;
[[vk::binding(0, 1)]]
RWTexture2D<float4> multiScatterLUT;

groupshared float3 s_fms[SPHERE_SAMPLES];
groupshared float3 s_lms[SPHERE_SAMPLES];

SamplerState g_sampler : register(s2, space0);

float rayToSphere(float h, float mu, float R)
{
    float disc = h * h * (mu * mu - 1.0) + R * R;
    if (disc < 0.0) return -1.0;
    float t = -h * mu + sqrt(max(0.0, disc));
    return t > 0.0 ? t : -1.0;  // don't clamp, preserve the "no hit" signal
}

float3 fibonacciDir(int i)
{
    float phi = acos(1.0 - 2.0 * (float(i) + 0.5) / float(SPHERE_SAMPLES));
    float theta = kPI * (1.0 + sqrt(5.0)) * float(i);
    return normalize(float3(sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta)));
}

[numthreads(1, 1, SPHERE_SAMPLES)]
void main(uint3 groupID : SV_GroupID, uint3 localID : SV_GroupThreadID)
{
    int2 lutSize;
    multiScatterLUT.GetDimensions(lutSize.x, lutSize.y);
    int2 coord = int2(groupID.xy); // one workgroup per output texel
    int rayIdx = int(localID.z);

    float2 uv = (float2(coord) + 0.5) / float2(lutSize);
    float h = uToHeight(uv.x, BOTTOM_RADIUS, TOP_RADIUS); // sqrt-warp matches atmosUV
    float cosSun = uv.y * 2.0 - 1.0;

    float sinSun = sqrt(max(0.0, 1.0 - cosSun * cosSun));
    float3 sunDir = float3(sinSun, cosSun, 0.0);
    float3 pos = float3(0.0, h, 0.0); // camera sits on Y axis at height h

    float3 rayDir = fibonacciDir(rayIdx);
    float mu = rayDir.y; // cosine of ray with zenith (planet centred)

    float tGround = rayToSphere(h, mu, BOTTOM_RADIUS);
    float tTop = rayToSphere(h, mu, TOP_RADIUS);
    float tMax = (tGround > 0.0) ? tGround : tTop; // tTop is -1 if no hit

    float3 fms = float3(0.0); // multiple-scatter factor (geometric series base)
    float3 lms = float3(0.0); // multiple-scatter luminance
    float3 T_view = float3(1.0); // transmittance accumulated along this ray

    if (tMax > 0.0)
    {
        float dt = tMax / float(STEPS);

        for (int i = 0; i < STEPS; i++)
        {
            float t = (float(i) + 0.5) * dt;
            float3 p = pos + rayDir * t;
            float alt = max(length(p) - BOTTOM_RADIUS, 0.0);

            // Density
            float rho_ray = exp(-alt / HEIGHT_RAY);
            float rho_mie = exp(-alt / HEIGHT_MIE);
            float denom = (HEIGHT_ABSORPTION - alt) / ABSORPTION_FALLOFF;
            float rho_oz = (1.0 / (denom * denom + 1.0)) * rho_ray;

            float3 sigma_s = RAY_BETA * rho_ray + MIE_BETA * rho_mie;
            float3 sigma_t = sigma_s + ABSORPTION_BETA * rho_oz;

            // Step transmittance
            float3 T_step = exp(-sigma_t * dt);

            // Sun transmittance at this point
            float cosSunHere = dot(normalize(p), sunDir);
            float3 T_sun = sampleTransmittance(transmittanceLUT, g_sampler, 
                                               length(p), cosSunHere,
                                               BOTTOM_RADIUS, TOP_RADIUS);

            const float ISO = 1.0 / (4.0 * kPI);
            float3 S = ISO * sigma_s * T_sun;
            // Analytic single-step luminance integral
            float3 Sint = (S - S * T_step) / max(sigma_t, float3(1e-7));

            fms += T_view * sigma_s * ISO * dt;
            lms += T_view * Sint;
            T_view *= T_step;
        }

        // Ground albedo bounce - Lambertian, albedo 0.3
        if (tGround > 0.0)
        {
            float3 gndPos = pos + rayDir * tGround;
            float cosGndSun = dot(normalize(gndPos), sunDir);
            float3 T_sun_gnd = sampleTransmittance(transmittanceLUT, g_sampler,
                                                   BOTTOM_RADIUS, cosGndSun,
                                                   BOTTOM_RADIUS, TOP_RADIUS);
            lms += T_view * (0.3 / kPI) * max(cosGndSun, 0.0) * T_sun_gnd;
        }
    }

    s_fms[rayIdx] = fms;
    s_lms[rayIdx] = lms;
    
    GroupMemoryBarrierWithGroupSync();

    // Only invocation 0 reduces and writes output
    if (rayIdx == 0)
    {
        float3 totalFms = float3(0.0);
        float3 totalLms = float3(0.0);
        for (int k = 0; k < SPHERE_SAMPLES; k++)
        {
            totalFms += s_fms[k];
            totalLms += s_lms[k];
        }
        // Average over sphere
        totalFms /= float(SPHERE_SAMPLES);
        totalLms /= float(SPHERE_SAMPLES);

        // Closed-form infinite geometric series: L_ms = lms / (1 - fms)
        float3 ms = totalLms / max(float3(1e-4), float3(1.0) - totalFms);
        multiScatterLUT[coord] = float4(ms, 1.0);
    }
}