struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertexMain(
    float3 pos : POSITION, 
    float3 inColor : COLOR,
    float2 inUv : TEXCOORD0
) {
    VertexOutput output;
    output.position = float4(pos, 1.0);
    output.color = inColor;
    output.uv = inUv;
    return output;
}

[[vk::binding(0, 0)]]
Texture2D<float4> sceneTexture;

[[vk::binding(1, 0)]]
SamplerState g_sampler;

[shader("fragment")]
float4 fsMain(VertexOutput input) : SV_Target
{
    float4 color = sceneTexture.SampleLevel(g_sampler, input.uv, 0.0);

    const float3 luminanceWeights = float3(0.2126, 0.7152, 0.0722);

    float luminance = dot(color.rgb, luminanceWeights);
    float3 grayscale = float3(luminance);
    
    float saturationAmount = 0.5; // You can pass this as a uniform or constant
    float3 finalColor = lerp(grayscale, color.rgb, saturationAmount);
    
    return float4(finalColor, color.a);
}