// CloudComposite.shader
// Composites the half-resolution cloud buffer (RGB=scattering, A=transmittance)
// over the current scene colour already in the swapchain attachment.
//
// Rendered as a fullscreen triangle at sky depth (z=0.9999) inside the main
// render-pass, after the atmosphere and sun passes.
//
// Descriptor layout (set = 0):
//   bind 0 : sampler2D  cloudBuffer  (half-res R16G16B16A16_SFLOAT, CLAMP)

Shader "SF/Clouds/CloudComposite"
{
    VertexShader
    {
        #version 450

        void main()
        {
            vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(uv * 2.0 - 1.0, 0.9999, 1.0);
        }
    }

    FragmentShader
    {
        #version 450

        layout(location = 0) out vec4 outColor;

        layout(set = 0, binding = 0) uniform sampler2D cloudBuffer;

        // Full-resolution screen size is needed to map gl_FragCoord correctly
        // onto the half-resolution cloud buffer. Passed from the same UBO used
        // by CloudRaymarch (screenSize holds the HALF-res dimensions there, so
        // we push full-res size separately here to avoid confusion).
        layout(set = 0, binding = 1) uniform CompositeUBO
        {
            vec2 fullScreenSize;   // full-res render target dimensions (pixels)
        } u;

        void main()
        {
            // Derive UV from full-res screen position so every full-res pixel
            // maps to the correct texel in the half-res cloud buffer.
            // No textureSize() arithmetic -- that breaks on odd-dimension targets.
            vec2 uv = gl_FragCoord.xy / u.fullScreenSize;

            vec4 cloud = texture(cloudBuffer, uv);

            // Fully transparent sample -- nothing to composite
            if (cloud.a > 0.999)
                discard;

            // cloud.rgb is pre-multiplied scattering; alpha = 1 - transmittance
            float alpha = 1.0 - cloud.a;
            outColor = vec4(cloud.rgb, alpha);
        }
        
    }
}
