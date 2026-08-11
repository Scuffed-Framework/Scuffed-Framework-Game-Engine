#define SHARED_SAMPLER_SET 1
#include "Common/Samplers.si"
#include "Atmosphere/Atmosphere.si"
#include "Common/Camera.si"

struct CloudUBO
{
    float cloudBottomRadius;
    float cloudTopRadius;
    float stepCount;
    float lightStepCount;

    float cloudDensityScale;
    float cloudCoverage;

    float time;

    float sdfRangeMetres;
    int frameIndex;

    float cloudDetailScale;
    float cloudBaseNoiseScale;
    float cloudCurlNoiseScale;

    float cloudWeatherUVScale;
    float percipitationBias;
    float FadeDistance2d;
    float fadeSmoothDist;

    float2 Wind;
    float Speed;
    float unused;
};

struct SSBOLensFlare
{
    float datas[4];  // Fixed size
};

struct PushConsts
{
    uint bCloud;
    uint bFog;
};

// Changed to RWStructuredBuffer for write access
[[vk::binding(0, 0)]]
RWStructuredBuffer<SSBOLensFlare> ssboLensFlareDatas;

[[vk::binding(1, 0)]]
ConstantBuffer<AtmoUBO> kAtmo;

[[vk::binding(2, 0)]]  // Fixed binding index conflict
ConstantBuffer<CloudUBO> kCloud;

[[vk::binding(3, 0)]]
Texture2D<float4> inDepth;

[[vk::binding(4, 0)]]
Texture2D<float4> inCloudFogReconstructionTexture;

[[vk::binding(5, 0)]]
Texture2D<float4> inCloudReconstructionTexture;

[[vk::binding(6, 0)]]
Texture2D<float4> inTransmittanceLut;

[[vk::binding(7, 0)]]
ConstantBuffer<Camera> kCam;

// Changed to accept float2 for UV comparison
bool onRange(float2 coord, float2 minVal, float2 maxVal)
{
    return all(coord >= minVal) && all(coord <= maxVal);
}

[[vk::push_constant]]
PushConsts pushConsts;

[shader("compute")]
[numthreads(1, 1, 1)]
void main()
{
    // Fixed float4 constructor
    float4 projectPos = mul(kCam.viewProjection, float4(kAtmo.sunDir.xyz * 9999999.0f, 1.0f));
    projectPos.xyz /= projectPos.w;

    projectPos.xy = 0.5 * projectPos.xy + 0.5;
    projectPos.y = 1.0 - projectPos.y;

    float2 sunUv = projectPos.xy;
    // Now passing float2 to onRange
    if(!onRange(projectPos.xy, float2(0.0), float2(1.0)) || projectPos.w > 0.0f)
    {
        ssboLensFlareDatas[0].datas[3] = 0.0f;
        return;
    }

    ssboLensFlareDatas[0].datas[3] = 1.0f;

    float4 cloudColor = inCloudReconstructionTexture.SampleLevel(linearClampEdgeSampler, sunUv, 0);
    float4 fogColor = inCloudFogReconstructionTexture.SampleLevel(linearClampEdgeSampler, sunUv, 0);
    float sceneZ = inDepth.SampleLevel(pointClampEdgeSampler, sunUv, 0).r;

    if(sceneZ <= 0.0f)
    {
        if(pushConsts.bCloud > 0)
        {
            ssboLensFlareDatas[0].datas[3] = cloudColor.a;
        }

        if(fogColor.a > 0.0f && pushConsts.bFog > 0)
        {
            ssboLensFlareDatas[0].datas[3] *= fogColor.a;
        }
    }
    else
    {
        ssboLensFlareDatas[0].datas[3] = 0.0f;
    }

    float3 atmosphereTransmittance;
    {
        float3 samplePos = float3(0.0, kCloud.cloudBottomRadius + (kCloud.cloudTopRadius - kCloud.cloudBottomRadius), 0.0);
        float sampleHeight = length(samplePos);

        const float3 upVector = samplePos / sampleHeight;
        float viewZenithCosAngle = dot(-normalize(kAtmo.sunDir.xyz), upVector);
        float2 sampleUv = atmosUV(sampleHeight, viewZenithCosAngle, kAtmo.bottomRadius, kAtmo.topRadius);
        atmosphereTransmittance = inTransmittanceLut.SampleLevel(linearClampEdgeSampler, sampleUv, 0).rgb;
    }

    // Writing to structured buffer correctly
    ssboLensFlareDatas[0].datas[0] = atmosphereTransmittance.x;
    ssboLensFlareDatas[0].datas[1] = atmosphereTransmittance.y;
    ssboLensFlareDatas[0].datas[2] = atmosphereTransmittance.z;
}