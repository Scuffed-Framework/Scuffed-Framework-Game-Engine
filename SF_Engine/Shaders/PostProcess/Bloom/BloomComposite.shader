// BloomComposite.shader : Pass 3 of 3.
// Additively overlays the blurred bloom texture onto the scene.
// The scene is already rendered and tonemapped on the swapchain;
// this pass simply adds the glow contribution on top.
//
// Blend fmode: additive (src=One, dst=One) so it accumulates without washing out.
//
// set=0  bind=0  texture2D bloomTex   (blurred bloom buffer, half-res)
// set=0  bind=1  UBO BloomCompositeUBO

struct BloomCompositeUBO
{
    float bloomlerp;   // additive weight (e.g. 0.04)
    float exposure;    // unused in additive fmode, kept for API compat
    float2 _pad;
};

[[vk::binding(0, 0)]]
Texture2D<float4> bloomTex;

[[vk::binding(1, 0)]]
ConstantBuffer<BloomCompositeUBO> u;

[[vk::binding(2, 0)]]
SamplerState g_sampler;

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

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
    float3 bloom = bloomTex.SampleLevel(g_sampler, input.uv, 0.0).rgb;
    // Pure additive overlay : scene is already on swapchain, we just add glow
    return float4(bloom * u.bloomlerp, 0.0);  // alpha=0 for additive blend
}