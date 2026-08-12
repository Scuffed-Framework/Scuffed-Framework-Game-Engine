// --- Bindings ---
[[vk::binding(0, 0)]] Sampler2D ImageFullRes;
[[vk::binding(0, 1)]] Sampler2D ImageHalfRes;
[[vk::binding(0, 2)]] Sampler2D ImageEighthRes;
[[vk::binding(0, 3)]] Sampler2D ImageSixteenthRes;
[[vk::binding(0, 4)]] Sampler2D ImageThirtySecondthRes;
[[vk::binding(0, 5)]] Sampler2D ImageSixtyFourthRes;

// Bloom parameters typically passed via a Constant Buffer
struct BloomParams
{
    float4 Threshold; // x: threshold, y: threshold - knee, z: 2 * knee, w: 0.25 / knee
    float Intensity;
    float Scatter;    // Controls the bloom radius/falloff during upsampling
};

[[vk::binding(1, 0)]] ConstantBuffer<BloomParams> u_BloomParams;

// --- Helper Functions ---

// Unity's Quadratic Thresholding (Soft Knee)
float3 Prefilter(float3 color)
{
    float brightness = max(color.r, max(color.g, color.b));
    float softness = clamp(brightness - u_BloomParams.Threshold.y, 0.0, u_BloomParams.Threshold.z);
    softness = (softness * softness) * u_BloomParams.Threshold.w;
    
    float contribution = max(brightness - u_BloomParams.Threshold.x, softness);
    contribution = max(contribution, 0.00001); // Prevent division by zero
    
    return color * (contribution / brightness);
}

// Unity 13-tap Downsample (Dual Kawase / Tent Filter)
float3 Downsample(Sampler2D tex, float2 uv, float2 texelSize)
{
    float4 d = texelSize.xyxy * float4(-1.0, -1.0, 1.0, 1.0);
    
    float3 s = tex.Sample(uv).rgb * 0.125;
    s += tex.Sample(uv + d.xw).rgb * 0.125;
    s += tex.Sample(uv + d.zw).rgb * 0.125;
    s += tex.Sample(uv + d.zy).rgb * 0.125;
    s += tex.Sample(uv + d.xy).rgb * 0.125;

    s += tex.Sample(uv + float2(d.x, 0.0)).rgb * 0.0625;
    s += tex.Sample(uv + float2(d.z, 0.0)).rgb * 0.0625;
    s += tex.Sample(uv + float2(0.0, d.y)).rgb * 0.0625;
    s += tex.Sample(uv + float2(0.0, d.w)).rgb * 0.0625;

    s += tex.Sample(uv + d.xw * 0.5).rgb * 0.0625;
    s += tex.Sample(uv + d.zw * 0.5).rgb * 0.0625;
    s += tex.Sample(uv + d.zy * 0.5).rgb * 0.0625;
    s += tex.Sample(uv + d.xy * 0.5).rgb * 0.0625;

    return s;
}

// Unity 9-tap Upsample (Tent Filter)
float3 Upsample(Sampler2D tex, float2 uv, float2 texelSize, float scatter)
{
    float4 d = texelSize.xyxy * float4(-1.0, -1.0, 1.0, 1.0) * scatter;
    
    float3 s = tex.Sample(uv).rgb * 0.25;
    s += tex.Sample(uv + d.xw).rgb * 0.1875;
    s += tex.Sample(uv + d.zw).rgb * 0.1875;
    s += tex.Sample(uv + d.zy).rgb * 0.1875;
    s += tex.Sample(uv + d.xy).rgb * 0.1875;
    
    return s;
}

// --- Shader Entry Points ---

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

// 1. Prefilter & Downsample Pass (Run this on the first downsample from FullRes)
[shader("fragment")]
float4 FS_Prefilter(VertexOutput input) : SV_Target
{
    // Assuming you calculate texel size in the engine and pass it, or calculate it here
    uint width, height;
    ImageFullRes.GetDimensions(width, height);
    float2 texelSize = 1.0 / float2(width, height);
    
    float3 color = Downsample(ImageFullRes, input.uv, texelSize);
    return float4(Prefilter(color), 1.0);
}

// 2. Standard Downsample Pass (Run this for 1/4 -> 1/8 -> 1/16 etc.)
[shader("fragment")]
float4 FS_Downsample(VertexOutput input, uniform Sampler2D sourceMip) : SV_Target
{
    uint width, height;
    sourceMip.GetDimensions(width, height);
    float2 texelSize = 1.0 / float2(width, height);
    
    return float4(Downsample(sourceMip, input.uv, texelSize), 1.0);
}

// 3. Final Combine Pass using your explicit bindings
// (Instead of ping-ponging upsample targets, this accumulates all mips in one pass)
[shader("fragment")]
float4 FS_FinalCombine(VertexOutput input) : SV_Target
{
    float3 finalBloom = float3(0.0, 0.0, 0.0);
    
    // Sample from each resolution tier.
    // In a pure Unity approach, these would be blended successively (e.g. 1/64 into 1/32, then into 1/16).
    // Using your bindings, we can do a weighted additive blend (scatter/falloff).
    
    float weight = 1.0;
    float weightSum = 0.0;
    
    // Additive sampling from all bound resolutions
    float3 b1 = ImageHalfRes.Sample(input.uv).rgb;
    float3 b2 = ImageEighthRes.Sample(input.uv).rgb;
    float3 b3 = ImageSixteenthRes.Sample(input.uv).rgb;
    float3 b4 = ImageThirtySecondthRes.Sample(input.uv).rgb;
    float3 b5 = ImageSixtyFourthRes.Sample(input.uv).rgb;
    
    // Weighted combine (mimics the energy conservation of a scatter curve)
    finalBloom += b1 * 0.5000;
    finalBloom += b2 * 0.2500;
    finalBloom += b3 * 0.1250;
    finalBloom += b4 * 0.0625;
    finalBloom += b5 * 0.0625;

    // Modulate by master intensity
    finalBloom *= u_BloomParams.Intensity;
    
    // Sample the original un-bloomed image
    float3 baseColor = ImageFullRes.Sample(input.uv).rgb;
    
    // Unity applies bloom using additive blending, but often in linear space
    return float4(baseColor + finalBloom, 1.0);
}