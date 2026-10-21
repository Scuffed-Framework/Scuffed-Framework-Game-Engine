#include "Common/Samplers.si"
[vk::binding(0, 0)] Texture2D<float4> InTexture;
[vk::binding(1, 0)] RWTexture2D<float4> OutTexture;

[vk::binding(2, 0)] ConstantBuffer<PC> pc;

struct PC
{
    float2 InSize;
    float2 OutSize;
    bool Clamp;
    bool UseLinearInsteadOfBillinear; // Interpreted here as 1-tap linear vs 4-tap box
};

[numthreads(16, 16, 1)]
[shader("compute")]
void main(uint3 dispatch: SV_DispatchThreadID)
{
    // Out-of-bounds guard for target resolution
    if (dispatch.x >= (uint)pc.OutSize.x || dispatch.y >= (uint)pc.OutSize.y)
        return;

    // Pick sampler based on push constant clamp configuration
    SamplerState s = pc.Clamp ? linearClampEdgeSampler : linearClampBorder0000Sampler;

    // Calculate normalized UV for the center of the current destination pixel
    float2 outUV = (float2(dispatch.xy) + 0.5f) / pc.OutSize;

    float4 color = float4(0.0f, 0.0f, 0.0f, 1.0f);

    if (pc.UseLinearInsteadOfBillinear)
    {
        // Single tap hardware linear filtering (fast, but can blur/miss details depending on scale ratio)
        color = InTexture.SampleLevel(s, outUV, 0);
    }
    else
    {
        // High-quality 4-tap bilinear box filter (ideal for a 2x downsample)
        // Offsets UVs by half a texel in the input texture space to average 4 distinct neighborhoods
        float2 texelSize = 1.0f / pc.InSize;
        float2 offset = texelSize * 0.5f;

        float4 d0 = InTexture.SampleLevel(s, outUV + float2(-offset.x, -offset.y), 0);
        float4 d1 = InTexture.SampleLevel(s, outUV + float2(offset.x, -offset.y), 0);
        float4 d2 = InTexture.SampleLevel(s, outUV + float2(-offset.x, offset.y), 0);
        float4 d3 = InTexture.SampleLevel(s, outUV + float2(offset.x, offset.y), 0);

        color = (d0 + d1 + d2 + d3) * 0.25f;
    }

    // Write final downsampled pixel to storage image
    OutTexture[dispatch.xy] = color;
}
