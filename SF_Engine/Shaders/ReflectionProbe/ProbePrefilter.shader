// ProbePrefilter.shader : GGX-prefilters the probe's mip 0 into one lower mip (all 6 faces).
//
// Dispatched once per mip m >= 1. Nothing is passed in: the roughness for this mip is recovered
// from the ratio of the source size to the destination size, so it always agrees with
// ProbeRoughnessToMip() in ReflectionProbe.si:
//     m = log2(srcSize / dstSize),   perceptualRoughness = m / kProbeMaxMip
//
// Classic split-sum prefilter (N = V = R): importance-sample the GGX lobe around the output
// direction, weight each sample by N.L. Source is mip 0 only, sampled at LOD 0 with a Hammersley
// set -- at 128^2 faces and 96 samples the cost is negligible and the result is deterministic
// (no temporal noise to clean up).
//
// Resource layout (set=0), plus the shared sampler set (set=1) from Common/Samplers.si:
//   binding 0   TextureCube              probeSrc   (view of mip 0 only)
//   binding 1   RWTexture2DArray<float4> probeDst   (view of mip m, 6 layers)
#include "ReflectionProbe/ReflectionProbe.si"
#include "Common/Samplers.si"

[[vk::binding(0, 0)]] TextureCube probeSrc;
[[vk::binding(1, 0)]] RWTexture2DArray<float4> probeDst;

static const uint kSampleCount = 96u;
static const float kPi = 3.14159265358979;

float RadicalInverseVdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint dw, dh, dl;
    probeDst.GetDimensions(dw, dh, dl);
    if (id.x >= dw || id.y >= dh || id.z >= 6u)
        return;

    uint sw, sh;
    probeSrc.GetDimensions(sw, sh);

    float mip = log2(float(sw) / float(dw));
    float roughness = saturate(mip / kProbeMaxMip);
    float a = roughness * roughness;
    float a2 = a * a;

    float2 uv = (float2(id.xy) + 0.5) / float2(float(dw), float(dh));
    float3 N = ProbeCubeDir(id.z, uv);
    float3 V = N;

    float3 up = (abs(N.y) < 0.999) ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);

    float3 sum = float3(0.0, 0.0, 0.0);
    float wsum = 0.0;
    for (uint i = 0u; i < kSampleCount; ++i)
    {
        float2 Xi = float2(float(i) / float(kSampleCount), RadicalInverseVdC(i));

        float phi = 2.0 * kPi * Xi.x;
        float cosT = sqrt((1.0 - Xi.y) / (1.0 + (a2 - 1.0) * Xi.y));
        float sinT = sqrt(max(1.0 - cosT * cosT, 0.0));
        float3 H = T * (sinT * cos(phi)) + B * (sinT * sin(phi)) + N * cosT;

        float3 L = normalize(2.0 * dot(V, H) * H - V);
        float NoL = dot(N, L);
        if (NoL > 0.0)
        {
            sum += probeSrc.SampleLevel(linearClampEdgeMipFilterSampler, L, 0.0).rgb * NoL;
            wsum += NoL;
        }
    }

    float3 col = (wsum > 0.0) ? (sum / wsum) : probeSrc.SampleLevel(linearClampEdgeMipFilterSampler, N, 0.0).rgb;
    probeDst[uint3(id.xy, id.z)] = float4(col, 1.0);
}
