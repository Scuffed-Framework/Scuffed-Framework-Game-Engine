#include "Clouds/CloudCommon.si"

// Helper function to get world position from UV and depth
float3 getWorldPosFromUV(float2 uv, float depth)
{
    // Clamp depth to 1e-7 so it evaluates to an incredibly far distance,
    // but prevents the w-component from hitting exactly 0.0
    float safeDepth = max(depth, 1e-5);

    float4 clipSpace = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, safeDepth, 1.0);
    float4 viewPosH = mul(kAtmo.invProj, clipSpace);
    float3 viewSpacePos = viewPosH.xyz / viewPosH.w;
    float4 worldPos = mul(kAtmo.invView, float4(viewSpacePos, 1.0));
    return worldPos.xyz;
}

// Helper to check if a value is within range
bool isInRange(float2 value, float2 minVal, float2 maxVal)
{
    return all(value >= minVal) && all(value <= maxVal);
}

[shader("compute")]
[numthreads(8,8,1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // Get texture dimensions using HLSL method
    int2 texSize;
    imageCloudReconstructionTexture.GetDimensions(texSize.x, texSize.y);
    int2 workPos = int2(dispatchThreadID.xy);

    if(workPos.x >= texSize.x || workPos.y >= texSize.y)
    {
        return;
    }

    const float2 uv = (float2(workPos) + float2(0.5f)) / float2(texSize);
    
    // Fetch cloud depth - using HLSL Load syntax
    int2 depthTexSize;
    inCloudDepthTexture.GetDimensions(depthTexSize.x, depthTexSize.y);
    int2 depthReadPos = workPos / 4;
    float traceCloudDepth = inCloudDepthTexture.Load(int3(depthReadPos, 0)).r;
    
    int2 renderTexSize;
    inCloudRenderTexture.GetDimensions(renderTexSize.x, renderTexSize.y);
    const float2 curEvaluateCloudTexelSize = 1.0f / float2(renderTexSize);

    // Reproject to get prev uv.
    float3 worldPosCur = getWorldPosFromUV(uv, traceCloudDepth);
    float4 projPosPrev = mul(kCam.prevViewProjection, float4(worldPosCur, 1.0));
    float3 projPosPrevH = projPosPrev.xyz / projPosPrev.w;

    float2 uvPrev = projPosPrevH.xy * 0.5 + 0.5;
    uvPrev.y = 1.0 - uvPrev.y;

    // Valid check.
    const bool bPrevUvValid = isInRange(uvPrev, float2(0.0), float2(1.0));

    float4 color = float4(0.0);
    float4 fog = float4(0.0);
    float depthZ = 0.0;
    
    if(bPrevUvValid)
    {
        // Evaluate, fetch it.
        int2 readPos = workPos / 4;
        float4 curColor = inCloudRenderTexture.Load(int3(readPos, 0));
        float4 curFog = inCloudFogRenderTexture.Load(int3(readPos, 0));
        float curDepthZ = inCloudDepthTexture.Load(int3(readPos, 0)).r;

        // Sample history texture
        int2 historyTexSize;
        inCloudDepthReconstructionTextureHistory.GetDimensions(historyTexSize.x, historyTexSize.y);
        float preDepthZ = inCloudDepthReconstructionTextureHistory.SampleLevel(linearClampEdgeSampler, uvPrev, 0).r;

        // Evaluate state check.
        uint bayerIndex = kCloud.frameIndex.x % 16;
        int bayerValue = kBayer4x4[bayerIndex];
        int2 bayerOffset = int2(bayerValue % 4, bayerValue / 4);
        int2 workDeltaPos = workPos % 4;
        const bool bUpdateEvaluate = (workDeltaPos.x == bayerOffset.x) && (workDeltaPos.y == bayerOffset.y);
        
        if(bUpdateEvaluate)
        {
            depthZ = curDepthZ;
            // Just update color is good enough.
            color = curColor;
            fog = curFog;
        }
        else
        {
            // Prev uv valid, sample history with prev Uv.
            color = inCloudReconstructionTextureHistory.SampleLevel(linearClampEdgeSampler, uvPrev, 0);
            fog = inCloudFogReconstructionTextureHistory.SampleLevel(linearClampEdgeSampler, uvPrev, 0);
            depthZ = preDepthZ;
        }
    }
    else
    {
        // No history valid, no evaluate, bilinear sample current.
        color = inCloudRenderTexture.SampleLevel(linearClampEdgeSampler, uv, 0);
        fog = inCloudFogRenderTexture.SampleLevel(linearClampEdgeSampler, uv, 0);
        depthZ = inCloudDepthTexture.SampleLevel(linearClampEdgeSampler, uv, 0).r;
    }

    if (any(anyBadFloat(color)) || any(anyBadFloat(color)))
    {
        color = float4(0.0, 0.0, 0.0, 1.0);
    }
    if (any(anyBadFloat(fog)) || any(anyBadFloat(fog)))
    {
        fog = float4(0.0, 0.0, 0.0, 1.0);
    }
    if (anyBadFloat(depthZ) || anyBadFloat(depthZ))
    {
        depthZ = 0.0;
    }

    // Store results
    imageCloudReconstructionTexture[workPos] = color;
    imageCloudFogReconstructionTexture[workPos] = fog;
    imageCloudDepthReconstructionTexture[workPos] = float4(depthZ, 0.0, 0.0, 0.0);
}