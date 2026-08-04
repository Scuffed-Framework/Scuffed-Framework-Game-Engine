#include "Vertout.si"

[shader("vertex")]
VertexOutput vertexMain(
    float3 pos : POSITION, 
    float3 inColor : COLOR,
    float2 inUv : TEXCOORD0
) {
    VertexOutput output;
    output.position = float4(pos, 1.0);
    output.uv = inUv;
    output.color = inColor;
    return output;
}

[[vk::binding(0, 0)]]
Texture2D<float4> colorSampler;

[[vk::binding(1, 0)]]
SamplerState g_sampler;

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target
{
    return colorSampler.SampleLevel(g_sampler, input.uv, 0.0);
}