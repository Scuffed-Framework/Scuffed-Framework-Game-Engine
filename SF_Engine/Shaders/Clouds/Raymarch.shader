#include "Noise/BlueNoise.si"
#include "Clouds/Clouds.si"

// Constants
#define BAYER_MATRIX_SIZE 16
const int kBayerMatrix16[16] = int[16](
    0, 8, 2, 10,
    12, 4, 14, 6,
    3, 11, 1, 9,
    15, 7, 13, 5
);

// Output textures
[[vk::binding(0, 0)]]
RWTexture2D<float4> imageCloudRenderTexture;

[[vk::binding(1, 0)]]
RWTexture2D<float> imageCloudDepthTexture;

[[vk::binding(2, 0)]]
RWTexture2D<float4> imageCloudFogRenderTexture;

[[vk::binding(3, 0)]]
ConstantBuffer<AtmoUBO> u;

// Noise functions
float rand3DPCG16(int3 p)
{
    int3 q = p * int3(1664525, 1664525, 1664525) + int3(1013904223, 1013904223, 1013904223);
    int x = q.x ^ (q.y * 65536) ^ (q.z * 65536);
    return float(x & 0xFFFF) / 65536.0;
}

float interleavedGradientNoise(float2 position, float frameIndex)
{
    float2 n = position + float2(0.5, 0.5);
    float2 f = fract(n);
    
    // Optimized interleaved gradient noise
    float a = dot(f, float2(0.06711056, 0.00583715));
    float b = dot(floor(n), float2(0.06711056, 0.00583715))     ;
    return fract(a + b + frameIndex * 0.001);
}

float bayer64(float2 p)
{
    uint x = uint(p.x) & 7u;
    uint y = uint(p.y) & 7u;
    
    // Bayer matrix 8x8
    const uint bayer8x8[64] = uint[64](
        0, 32, 8, 40, 2, 34, 10, 42,
        48, 16, 56, 24, 50, 18, 58, 26,
        12, 44, 4, 36, 14, 46, 6, 38,
        60, 28, 52, 20, 62, 30, 54, 22,
        3, 35, 11, 43, 1, 33, 9, 41,
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47, 7, 39, 13, 45, 5, 37,
        63, 31, 55, 23, 61, 29, 53, 21
    );
    
    return float(bayer8x8[y * 8u + x]) / 64.0;
}

// Cloud ray marching and color computation
float4 cloudColorCompute(
    float2 uv,
    float noiseSeed,
    out float depth,
    int2 workPos,
    float3 worldDir,
    bool godRays,
    out float4 fogLighting,
    float blueNoise
)
{
    // Initialize outputs
    depth = 1.0; // Reverse Z (far plane)
    fogLighting = float4(0.0);
    
    // Cloud parameters
    float cloudAltitude = cloud.cloudBottomRadius; // meters
    float cloudCoverage = cloud.cloudCoverage; // Replace with actual frameData value if available
    float cloudDensity = cloud.cloudDensityScale; // Replace with actual frameData value if available
    
    // Ray origin from camera position
    float3 rayOrigin = u.cameraPos.xyz;
    float3 rayDir = worldDir;

    float cloudThickness = cloud.cloudTopRadius - cloud.cloudBottomRadius;
    
    // Calculate intersection with cloud layer
    float cloudHeightStart = cloudAltitude;
    float cloudHeightEnd = cloudAltitude + cloudThickness;
    
    // Distance to cloud layer
    float tMin = 0.0;
    float tMax = 100000.0;
    
    // Find intersection with cloud layer (simplified)
    float3 planetCenter = u.planetPos.xyz;
    float planetRadius = u.bottomRadius;
    
    // Check for cloud layer intersection
    float cloudDist = (cloudHeightStart - rayOrigin.y) / rayDir.y;
    float cloudDistEnd = (cloudHeightEnd - rayOrigin.y) / rayDir.y;
    
    if (cloudDist < 0.0 || cloudDistEnd < 0.0)
    {
        return float4(0.0);
    }
    
    tMin = min(cloudDist, cloudDistEnd);
    tMax = max(cloudDist, cloudDistEnd);
    
    if (tMin < 0.0) tMin = 0.0;
    
    // Ray march through cloud
    float stepSize = (tMax - tMin) / 64.0; // 64 steps
    float totalDensity = 0.0;
    float3 accumulatedLight = float3(0.0);
    float3 accumulatedFog = float3(0.0);
    
    // Sample cloud density using noise
    for (int i = 0; i < 64; i++)
    {
        float t = tMin + float(i) * stepSize + blueNoise * stepSize;
        float3 samplePos = rayOrigin + rayDir * t;
        
        // Calculate cloud density at this position
        float height = length(samplePos - planetCenter) - planetRadius;
        float localHeight = height - cloudAltitude;
        
        if (localHeight < 0.0 || localHeight > cloudThickness)
        {
            continue;
        }
        
        // Noise-based density (simplified - in practice use 3D noise)
        float density = 1.0 - abs(localHeight / cloudThickness - 0.5) * 2.0;
        density *= cloudDensity * 0.1;
        
        // Coverage mask (simplified)
        float coverage = cloudCoverage;
        density *= coverage;
        
        // Calculate transmittance
        float transmittance = exp(-density * stepSize * 0.5);
        
        // Light scattering (simplified)
        float3 lightDir = u.sunDir.xyz;
        float cosAngle = dot(rayDir, lightDir);
        
        // Phase function (Henyey-Greenstein)
        float g = HenyeyGreensteinPhaseFunction(cosAngle, 0.6);
        float phase = (1.0 - g * g) / (4.0 * 3.14159 * pow(1.0 + g * g - 2.0 * g * cosAngle, 1.5));
        
        // Sun light contribution
        float3 sunColor = u.sunDir.www * 2.0;
        float3 lightContrib = sunColor * phase * density * stepSize;
        
        // Accumulate
        accumulatedLight += lightContrib * exp(-totalDensity);
        accumulatedFog += float3(0.1) * density * stepSize;
        totalDensity += density * stepSize;
        
        // Early exit if fully opaque
        if (totalDensity > 3.0)
        {
            break;
        }
    }
    
    // Calculate depth (reverse Z)
    float distance = tMin + totalDensity * 0.5;
    float4 clipPos = mul(u.invProj, float4(rayOrigin + rayDir * distance, 1.0));
    depth = clipPos.z / clipPos.w;
    
    // Convert from linear depth to reverse Z (0=far, 1=near)
    depth = 1.0 - depth;
    
    // Output color with alpha
    float3 cloudColor = accumulatedLight * 0.5;
    float alpha = 1.0 - exp(-totalDensity * 0.3);
    
    fogLighting = float4(accumulatedFog, alpha);
    
    return float4(cloudColor, alpha);
}

[numthreads(8, 8, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int2 texSize;
    imageCloudRenderTexture.GetDimensions(texSize.x, texSize.y);
    int2 workPos = int2(globalThreadID.xy);

    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
    {
        return;
    }

    // Get bayer offset matrix
    uint bayerIndex = uint(0) % 16u; // frameData.frameIndex.x not available
    int2 bayerOffset = int2(kBayerMatrix16[bayerIndex] % 4, kBayerMatrix16[bayerIndex] / 4);

    // Get evaluate position in full resolution
    int2 fullResSize = texSize * 4;
    int2 fullResWorkPos = workPos * 4 + bayerOffset;

    // Get evaluate uv in full resolution
    const float2 uv = (float2(fullResWorkPos) + float2(0.5)) / float2(fullResSize);

    // Use blue noise texture (placeholder)
    float blueNoise = BlueNoiseErrorDistrib(
        uint(workPos.x), 
        uint(workPos.y), 
        0, 
        0u
    );

    // Offset retarget for new seeds each frame
    uint2 offset = uint2(float2(0.754877669, 0.569840296) * 0.0) * uint2(texSize); // frameData.frameIndex.x not available
    uint2 offsetId = uint2(workPos) + offset;
    offsetId.x = offsetId.x % uint(texSize.x);
    offsetId.y = offsetId.y % uint(texSize.y);
    
    float blueNoise2 = BlueNoiseErrorDistrib(
        offsetId.x, 
        offsetId.y, 
        0, 
        0u
    );

    // Calculate world space direction
    float4 clipSpace = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    float4 viewPosH = mul(u.invProj, clipSpace);
    float3 viewSpaceDir = viewPosH.xyz / viewPosH.w;
    float3 worldDir = normalize(mul(u.invView, float4(viewSpaceDir, 0.0)).xyz);

    float depth = 0.0; // reverse z
    float4 fogLighting = float4(0.0);

    // Compute cloud color
    float4 cloudColor = cloudColorCompute( 
        uv, 
        blueNoise2, 
        depth, 
        workPos, 
        worldDir, 
        false, // godRays - replace with actual value
        fogLighting, 
        blueNoise2
    );

    // Store outputs
    imageCloudRenderTexture[workPos] = cloudColor;
    imageCloudDepthTexture[workPos] = depth;
    imageCloudFogRenderTexture[workPos] = fogLighting;
}