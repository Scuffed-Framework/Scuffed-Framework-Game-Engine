#include "Clouds/CloudScatterFunctions.si"
#include "Clouds/Clouds.si"
#include "Common/Camera.si"
#include "Common/Math.si"

#define BAYER_MATRIX_SIZE 16
const static int kBayerMatrix16[BAYER_MATRIX_SIZE] = int[BAYER_MATRIX_SIZE](
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5
);

[[vk::binding(0, 0)]]
[[vk::image_format("r32f")]]
Texture2D<float> inCloudDepthTexture;

[[vk::binding(1, 0)]]
[[vk::image_format("rgba16f")]]
Texture2D<float4> inCloudRenderTexture;

[[vk::binding(2, 0)]]
[[vk::image_format("rgba16f")]]
Texture2D<float4> inCloudFogRenderTexture;

[[vk::binding(3, 0)]]
[[vk::image_format("rgba16f")]]
Texture2D<float4> inCloudReconstructionTextureHistory;

[[vk::binding(4, 0)]]
[[vk::image_format("rgba16f")]]
Texture2D<float4> inCloudFogReconstructionTextureHistory;

[[vk::binding(5, 0)]]
[[vk::image_format("r32f")]]
Texture2D<float> inCloudDepthReconstructionTextureHistory;

[[vk::binding(6, 0)]]
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> imageCloudReconstructionTexture;

[[vk::binding(7, 0)]]
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> imageCloudFogReconstructionTexture;

[[vk::binding(8, 0)]]
[[vk::image_format("r32f")]]
RWTexture2D<float> imageCloudDepthReconstructionTexture;

[[vk::binding(9, 0)]]
SamplerState linearClampEdgeSampler;

[[vk::binding(10, 0)]]
SamplerState pointClampEdgeSampler;

[[vk::binding(11, 0)]]
ConstantBuffer<Camera> camera;

// Utility function: Check if value is within range
bool onRange(float2 value, float2 minVal, float2 maxVal)
{
    return all(value >= minVal) && all(value <= maxVal);
}

// Utility function: Get world position from UV and depth
float3 getWorldPos(float2 uv, float depth, float4x4 currentViewProj)
{
    // Convert UV to NDC (Normalized Device Coordinates)
    float4 ndc = float4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    
    // Inverse projection and view matrices
    float4x4 invViewProj = inverse4x4(currentViewProj);
    float4 worldPos = mul(invViewProj, ndc);
    
    // Perspective divide
    return worldPos.xyz / worldPos.w;
}

// Variance clamping function for color reconstruction
float4 clampWithVariance(float4 preColor, float2 uv, float2 texelSize, Texture2D<float4> sampler)
{
    float wsum = 0.0;
    float4 vsum = float4(0.0);
    float4 vsum2 = float4(0.0);

    // 3x3 neighborhood
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleUV = uv + texelSize * float2(float(x), float(y));
            float4 neigh = sampler.SampleLevel(linearClampEdgeSampler, sampleUV, 0.0);
            float w = exp(-0.75 * (float(x) * float(x) + float(y) * float(y)));
            vsum2 += neigh * neigh * w;
            vsum += neigh * w;
            wsum += w;
        }
    }

    float4 ex = vsum / wsum;
    float4 ex2 = vsum2 / wsum;
    float4 dev = sqrt(max(ex2 - ex * ex, 0.0));

    const float boxSize = 2.5;
    float4 nmin = ex - dev * boxSize;
    float4 nmax = ex + dev * boxSize;

    return clamp(preColor, nmin, nmax);
}

// Main compute shader entry point
[numthreads(8, 8, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int2 texSize;
    imageCloudReconstructionTexture.GetDimensions(texSize.x, texSize.y);
    int2 workPos = int2(globalThreadID.xy);

    // Bounds check
    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
    {
        return;
    }

    const float2 uv = (float2(workPos) + float2(0.5)) / float2(texSize);
    
    int2 renderTexSize;
    inCloudRenderTexture.GetDimensions(renderTexSize.x, renderTexSize.y);
    const float2 curEvaluateCloudTexelSize = 1.0 / float2(renderTexSize);
    
    // Get current cloud depth (quarter-resolution)
    int2 depthSamplePos = workPos / 4;
    const float traceCloudDepth = inCloudDepthTexture.Load(int3(depthSamplePos, 0)).r;

    // Reproject to get previous UV
    float3 worldPosCur = getWorldPos(uv, traceCloudDepth, camera.viewProjection);
    float4 projPosPrev = mul(camera.prevViewProjection, float4(worldPosCur, 1.0));
    float3 projPosPrevH = projPosPrev.xyz / projPosPrev.w;

    float2 uvPrev = projPosPrevH.xy * 0.5 + 0.5;
    uvPrev.y = 1.0 - uvPrev.y;

    // Valid check for previous UV
    const bool bPrevUvValid = onRange(uvPrev, float2(0.0), float2(1.0));

    float4 color = float4(0.0);
    float4 fog = float4(0.0);
    float depthZ = 0.0;

    if (bPrevUvValid)
    {
        // Fetch current data
        float4 curColor = inCloudRenderTexture.Load(int3(depthSamplePos, 0));
        float4 curFog = inCloudFogRenderTexture.Load(int3(depthSamplePos, 0));
        float curDepthZ = inCloudDepthTexture.Load(int3(depthSamplePos, 0)).r;

        // Get previous depth from history
        float preDepthZ = inCloudDepthReconstructionTextureHistory.SampleLevel(linearClampEdgeSampler, uvPrev, 0.0).r;

        // Bayer pattern for sparse evaluation
        uint bayerIndex = uint(cloud.frameIndex.x) % 16u;
        int2 bayerOffset = int2(kBayerMatrix16[bayerIndex] % 4, kBayerMatrix16[bayerIndex] / 4);
        int2 workDeltaPos = workPos % 4;
        const bool bUpdateEvaluate = (workDeltaPos.x == bayerOffset.x) && (workDeltaPos.y == bayerOffset.y);

        if (bUpdateEvaluate)
        {
            depthZ = curDepthZ;
            
            // Update color with variance clamping
            if (abs(preDepthZ - curDepthZ) > 0.1)
            {
                color = curColor;
            }
            else
            {
                float4 preColor = inCloudReconstructionTextureHistory.SampleLevel(linearClampEdgeSampler, uvPrev, 0.0);
                
                // Apply variance clamping to history color
                float4 clampColorHistory = clampWithVariance(preColor, uv, curEvaluateCloudTexelSize, inCloudRenderTexture);
                
                color = lerp(clampColorHistory, curColor, 0.5);
            }

            // Update fog with variance clamping
            if (abs(preDepthZ - curDepthZ) > 0.1)
            {
                fog = curFog;
            }
            else
            {
                float4 preFog = inCloudFogReconstructionTextureHistory.SampleLevel(linearClampEdgeSampler, uvPrev, 0.0);
                
                // Apply variance clamping to history fog
                float4 clampFogHistory = clampWithVariance(preFog, uv, curEvaluateCloudTexelSize, inCloudFogRenderTexture);
                
                fog = lerp(clampFogHistory, curFog, 0.5);
            }
        }
        else
        {
            // No evaluation this frame, use history with reprojection
            color = inCloudReconstructionTextureHistory.SampleLevel(linearClampEdgeSampler, uvPrev, 0.0);
            fog = inCloudFogReconstructionTextureHistory.SampleLevel(linearClampEdgeSampler, uvPrev, 0.0);
            depthZ = preDepthZ;
        }
    }
    else
    {
        // No history valid, bilinear sample from current frame
        color = inCloudRenderTexture.SampleLevel(linearClampEdgeSampler, uv, 0.0);
        fog = inCloudFogRenderTexture.SampleLevel(linearClampEdgeSampler, uv, 0.0);
        depthZ = inCloudDepthTexture.SampleLevel(linearClampEdgeSampler, uv, 0.0).r;
    }

    // Write outputs
    imageCloudReconstructionTexture[workPos] = color;
    imageCloudFogReconstructionTexture[workPos] = fog;
    imageCloudDepthReconstructionTexture[workPos] = depthZ;
}