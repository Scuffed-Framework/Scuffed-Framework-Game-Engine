#include "VertexShaderCommon.si"

[shader("vertex")]
VSOutput vertexMain(
    float3 pos : POSITION, 
    float4 inColor : COLOR,
    float2 inUv : TEXCOORD0
) {
    VSOutput output;
    output.position = float4(pos, 1.0);
    output.uv0 = inUv;
    output.color = inColor;
    return output;
}

[[vk::binding(0, 0)]]
Sampler2D<float4> colorSampler;

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    return colorSampler.SampleLevel(input.uv0, 0.0);
}