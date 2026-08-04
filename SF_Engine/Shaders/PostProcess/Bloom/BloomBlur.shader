// BloomBlur.shader : Pass 2 of 3.
// 13-tap separable Gaussian blur (UE-style dual-pass).
// Run twice: first horizontal (direction=(1,0)), then vertical (direction=(0,1)).
//
// set=0  bind=0  texture2D srcTex      (bright-pass result or previous blur result)
// set=0  bind=1  UBO BloomBlurUBO
//
// Gaussian weights for sigma~2.0, 13 taps, normalised.

struct BloomBlurUBO
{
    float2 texelSize;    // 1.0 / textureSize(srcTex)
    float2 direction;    // (1,0) for horizontal, (0,1) for vertical
    float spread;        // blur radius scale (1.0 = standard, 2.0 = wider)
    float3 _pad;
};

[[vk::binding(0, 0)]]
Texture2D<float4> srcTex;

[[vk::binding(1, 0)]]
ConstantBuffer<BloomBlurUBO> u;

[[vk::binding(2, 0)]]
SamplerState g_sampler;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// 13-tap Gaussian weights (sigma ~2.0, summing to 1.0)
const int TAPS = 13;
const float W[13] = float[](
    0.00598, 0.02132, 0.05988, 0.13209, 0.22821,
    0.31062,
    0.22821, 0.13209, 0.05988, 0.02132, 0.00598,
    0.0, 0.0   // padding to fixed array size : unused
);
const int HALF = 5; // offsets run -5..+5 (11 taps used with the centre)

[shader("vertex")]
VSOutput vsMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
    return output;
}

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    float3 acc = float3(0.0);
    for (int i = -HALF; i <= HALF; i++)
    {
        float2 offset = u.direction * u.texelSize * float(i) * u.spread;
        acc += srcTex.SampleLevel(g_sampler, input.uv + offset, 0.0).rgb * W[i + HALF];
    }
    return float4(acc, 1.0);
}