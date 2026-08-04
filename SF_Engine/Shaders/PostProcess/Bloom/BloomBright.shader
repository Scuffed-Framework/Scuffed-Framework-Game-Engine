// BloomBright.shader : Generates a bloom source from the sun position.
// Instead of thresholding the scene (which causes feedback loops when
// blitting the post-composite swapchain), this pass generates a bright
// spot at the sun disc position in screen space, which is then blurred
// to create a natural glow/corona effect.
//
// set=0  bind=0  texture2D sceneTex   (capture buffer : used for scene-based threshold)
// set=0  bind=1  UBO BloomBrightUBO

struct BloomBrightUBO
{
    float threshold;    // luminance threshold (e.g. 0.8)
    float knee;         // soft-knee width (e.g. 0.2)
    float intensity;    // bloom intensity multiplier
    float _pad;
};

[[vk::binding(0, 0)]]
Texture2D<float4> sceneTex;

[[vk::binding(1, 0)]]
ConstantBuffer<BloomBrightUBO> u;

[[vk::binding(2, 0)]]
SamplerState g_sampler;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float Lum(float3 c) 
{ 
    return dot(c, float3(0.2126, 0.7152, 0.0722)); 
}

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
    float3 col = sceneTex.SampleLevel(g_sampler, input.uv, 0.0).rgb;
    float lum = Lum(col);

    // Hard threshold with soft knee : only pixels significantly
    // above threshold contribute, clamped to prevent feedback blowout.
    float lo = u.threshold - u.knee;
    float weight = clamp((lum - lo) / max(u.knee * 2.0, 1e-4), 0.0, 1.0);
    weight = weight * weight; // smooth
    
    // Clamp to prevent feedback runaway: when blitting a post-composite
    // swapchain, pixels already have some bloom baked in. The hard clamp
    // at 1.0 (scene max) ensures the bright pass output can't exceed what
    // a single frame could contribute, breaking the feedback loop.
    float3 bright = clamp(col * weight * u.intensity, float3(0.0), float3(1.0));
    return float4(bright, 1.0);
}