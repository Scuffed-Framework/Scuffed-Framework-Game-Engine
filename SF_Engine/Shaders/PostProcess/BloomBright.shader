// BloomBright.shader : Generates a bloom source from the sun position.
// Instead of thresholding the scene (which causes feedback loops when
// blitting the post-composite swapchain), this pass generates a bright
// spot at the sun disc position in screen space, which is then blurred
// to create a natural glow/corona effect.
//
// set=0  bind=0  sampler2D sceneTex   (capture buffer : used for scene-based threshold)
// set=0  bind=1  UBO BloomBrightUBO

Shader "SF/PostProcess/BloomBright"
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

        layout(set = 0, binding = 0) uniform sampler2D sceneTex;

        layout(set = 0, binding = 1) uniform BloomBrightUBO
        {
            float threshold;    // luminance threshold (e.g. 0.8)
            float knee;         // soft-knee width (e.g. 0.2)
            float intensity;    // bloom intensity multiplier
            float _pad;
        } u;

        float Lum(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

        void main()
        {
            vec3  col = texture(sceneTex, inUV).rgb;
            float lum = Lum(col);

            // Hard threshold with soft knee : only pixels significantly
            // above threshold contribute, clamped to prevent feedback blowout.
            float lo     = u.threshold - u.knee;
            float weight = clamp((lum - lo) / max(u.knee * 2.0, 1e-4), 0.0, 1.0);
            weight       = weight * weight; // smooth
            // Clamp to prevent feedback runaway: when blitting a post-composite
            // swapchain, pixels already have some bloom baked in. The hard clamp
            // at 1.0 (scene max) ensures the bright pass output can't exceed what
            // a single frame could contribute, breaking the feedback loop.
            vec3 bright = clamp(col * weight * u.intensity, vec3(0.0), vec3(1.0));
            outColor = vec4(bright, 1.0);
        }
    }
}
