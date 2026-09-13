// AtmosphereComposite.shader
//
// Blends Atmosphere.shader's compute output (imgAtmoColor, written earlier
// this frame in AtmospherePipelinePass::PreRender) into "hdr" as an actual
// subpass draw, inside stage 1's render pass, in AtmospherePipelinePass::
// Render() - after DeferredLight's subpass has already run and after the
// render pass's own clear has already happened.
//
// Relies on the pipeline's PremultipliedAlpha blend state:
//   finalRGB = srcRGB + dstRGB * (1 - srcA)
// Sky pixels (Atmosphere.shader wrote alpha=1)      -> finalRGB = srcRGB
// Geometry pixels (alpha=luminance(transmittance))  -> finalRGB = scatter + surface*transmit
// matching the same convention SSR's Composite.shader already uses.

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv0 : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID)
{
    VSOutput output;
    float2 uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv0 = uv;
    return output;
}

[[vk::binding(1, 0)]]
Sampler2D<float4> atmoColorSampler;

[shader("fragment")]
float4 fsMain(VSOutput input) : SV_Target
{
    return atmoColorSampler.SampleLevel(input.uv0, 0.0);
}
