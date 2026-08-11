#include "../Clouds/CloudCommon.si"
#include "../Atmosphere/Atmosphere.si"
#include "../Noise/BlueNoise.si"

float interleavedGradientNoise(float2 position, float frameIndex)
{
    float2 n = position + float2(0.5, 0.5);
    float2 f = frac(n);

    float a = dot(f, float2(0.06711056, 0.00583715));
    float b = dot(floor(n), float2(0.06711056, 0.00583715));
    return frac(a + b + frameIndex * 0.001);
}

// Helper functions for shadow mapping
float3 projectPos(float3 worldPos, float4x4 viewProj)
{
    float4 clipPos = mul(viewProj, float4(worldPos, 1.0));
    return clipPos.xyz / clipPos.w;
}

bool onRange(float3 coord, float3 minVal, float3 maxVal)
{
    return all(coord >= minVal) && all(coord <= maxVal);
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 texSize;
    imageHdrSceneColor.GetDimensions(texSize.x, texSize.y);

    int2 depthTextureSize;
    inDepth.GetDimensions(depthTextureSize.x, depthTextureSize.y);
    
    int2 workPos = int2(dispatchThreadID.xy);

    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
    {
        return;
    }

    const float2 uv = (float2(workPos) + float2(0.5f)) / float2(texSize);

    // Offset retarget for new seeds each frame
    uint2 offset = uint2(float2(0.754877669, 0.569840296) * kCloud.frameIndex.x * uint2(texSize));
    uint2 offsetId = workPos.xy + offset;
    offsetId.x = offsetId.x % texSize.x;
    offsetId.y = offsetId.y % texSize.y;
    float blueNoise2 = BlueNoiseErrorDistrib(offsetId.x, offsetId.y, 0, 0u);

    float4 clipSpace = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0, 1.0);
    float4 viewPosH = mul(kAtmo.invProj, clipSpace);
    float3 viewDir = viewPosH.xyz / viewPosH.w;
    float3 worldDir = normalize(mul(kAtmo.invView, float4(viewDir, 0.0)).xyz);

    // Direct RWTexture2D access (no imageLoad needed)
    float4 srcColor = imageHdrSceneColor[workPos];
    
    // Fix texture sampling
    float sceneZ = inDepth.SampleLevel(pointClampEdgeSampler, uv, 0).x;
    
    // Use .SampleLevel() instead of texture(sampler2D(...))
    float4 cloudColor = inCloudReconstructionTexture.SampleLevel(linearClampEdgeSampler, uv, 0);
    float4 fogColor = inCloudFogReconstructionTexture.SampleLevel(linearClampEdgeSampler, uv, 0);

    if (any(anyBadFloat(cloudColor)) || any(anyBadFloat(cloudColor)))
    {
        cloudColor = float4(0.0, 0.0, 0.0, 1.0);
    }
    if (any(anyBadFloat(fogColor)) || any(anyBadFloat(fogColor)))
    {
        fogColor = float4(0.0, 0.0, 0.0, 1.0);
    }

    float3 result = srcColor.rgb;

    if (sceneZ <= 0.0f) // reverse z
    {
        result = srcColor.rgb * cloudColor.a + cloudColor.rgb;

        if (fogColor.a >= 0.0f)
        {
            result.rgb = result.rgb * fogColor.a + max(float3(0.0f), fogColor.rgb);
        }
    }

    // God rays (shadow mapping removed)
    {
        const uint kGodRaySteps = 64;
        const float kMaxMarchingDistance = 400.0f;

        float4 clipSpace2 = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0, 1.0);
        float4 viewPosH2 = mul(kAtmo.invProj, clipSpace2);
        float3 viewSpaceDir = viewPosH2.xyz / viewPosH2.w;
        float3 worldDir2 = normalize(mul(kAtmo.invView, float4(viewSpaceDir, 0.0)).xyz);

        float3 worldPosWP = kCam.cameraPosition.xyz;;

        float pixelToCameraDistanceWP = max(1e-5f, length(worldPosWP));
        float3 rayDirWP = worldPosWP / pixelToCameraDistanceWP;

        float marchingDistance = min(kMaxMarchingDistance, pixelToCameraDistanceWP);
        if (pixelToCameraDistanceWP > kMaxMarchingDistance)
        {
            worldPosWP = kCam.cameraPosition.xyz - rayDirWP * marchingDistance;
        }

        float3 sunDirection = normalize(kAtmo.sunDir.xyz);
        float VoL = dot(worldDir2, sunDirection);

        float stepLength = marchingDistance / float(kGodRaySteps);
        float3 stepRay = rayDirWP * stepLength;

        float taaOffset = interleavedGradientNoise(workPos, kCloud.frameIndex.x * 0.1f);

        float3 rayPosWP = worldPosWP + stepRay * (blueNoise2 + 0.05);

        float transmittance2 = 1.0;
        float3 scatteredLight2 = float3(0.0, 0.0, 0.0);

        float miePhaseValue = hgPhase(0.8, -VoL);
        float rayleighPhaseValue = rayleighPhase(VoL);

        float3 groundToCloudTransfertIsoScatter;
        {
            float3 P0 = kCam.cameraPosition.xyz * 0.001 + float3(0.0, kAtmo.bottomRadius, 0.0);
            float viewHeight = length(P0);
            float3 upVector = P0 / viewHeight;
            float cosSunZenith = dot(upVector, sunDirection);
            float2 sampleUv = atmosUV(viewHeight, cosSunZenith, kAtmo.bottomRadius, kAtmo.topRadius);
            groundToCloudTransfertIsoScatter = inMultiScatterLUT.SampleLevel(sampleUv, 0.0).rgb;
        }

        for (uint i = 0; i < kGodRaySteps; i++)
        {
            // Shadow mapping completely removed - visibility always 1.0
            float visibilityTerm = 1.0;

            float3 atmosphereTransmittance;
            {
                float3 P0 = rayPosWP * 0.001 + float3(0.0, kAtmo.bottomRadius, 0.0);
                float viewHeight = length(P0);
                const float3 upVector = P0 / viewHeight;

                float viewZenithCosAngle = dot(sunDirection, upVector);
                float2 sampleUv = atmosUV(viewHeight, viewZenithCosAngle, kAtmo.bottomRadius, kAtmo.topRadius);
                atmosphereTransmittance = inTransmittanceLut.SampleLevel(linearClampEdgeSampler, sampleUv, 0).rgb;
            }

            float density = getDensity(distance(rayPosWP, kCam.cameraPosition.xyz));

            float sigmaS = density;
            float sigmaE = max(sigmaS, 1e-8f);

            float3 phaseTimesScattering = float3(miePhaseValue + rayleighPhaseValue);
            float3 sunSkyLuminance = groundToCloudTransfertIsoScatter + visibilityTerm * float3(1) * phaseTimesScattering * atmosphereTransmittance;

            float3 sactterLitStep = sunSkyLuminance * sigmaS;

            float stepTransmittance = exp(-sigmaE * stepLength);
            scatteredLight2 += transmittance2 * (sactterLitStep - sactterLitStep * stepTransmittance) / sigmaE;
            transmittance2 *= stepTransmittance;

            rayPosWP += stepRay;
        }

        result.rgb = result.rgb * transmittance2 + scatteredLight2;
    }

    // Direct RWTexture2D write (no imageStore needed)
    imageHdrSceneColor[workPos] = float4(result.rgb, 1.0);
    // imageHdrSceneColor[workPos] = float4(1.0, 0.0, 0.0, 1.0); // solid red, unconditional
}