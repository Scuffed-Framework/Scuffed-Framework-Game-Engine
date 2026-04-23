// BloomComposite.shader : Pass 3 of 3.
// Additively overlays the blurred bloom texture onto the scene.
// The scene is already rendered and tonemapped on the swapchain;
// this pass simply adds the glow contribution on top.
//
// Blend mode: additive (src=One, dst=One) so it accumulates without washing out.
//
// set=0  bind=0  sampler2D bloomTex   (blurred bloom buffer, half-res)
// set=0  bind=1  UBO BloomCompositeUBO

Shader "SF/PostProcess/BloomComposite"
{
    VertexShader
    {
        #version 450
        layout(location = 0) out vec2 outUV;
        void main()
        {
            outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
        }
    }

    FragmentShader
    {
        #version 450
        layout(location = 0) in  vec2 inUV;
        layout(location = 0) out vec4 outColor;

        layout(set = 0, binding = 0) uniform sampler2D bloomTex;

        layout(set = 0, binding = 1) uniform BloomCompositeUBO
        {
            float bloomMix;   // additive weight (e.g. 0.04)
            float exposure;   // unused in additive mode, kept for API compat
            vec2  _pad;
        } u;

        void main()
        {
            vec3 bloom = texture(bloomTex, inUV).rgb;
            // Pure additive overlay : scene is already on swapchain, we just add glow
            outColor = vec4(bloom * u.bloomMix, 0.0);  // alpha=0 for additive blend
        }
    }
}
