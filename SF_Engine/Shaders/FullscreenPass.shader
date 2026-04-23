// FullscreenPass.shader
// Renders a single fullscreen triangle (no vertex buffer needed).
// The fragment stage samples from a color attachment (e.g. a resolved scene image).
// Bind the source image to set=0, binding=1 as a combined image sampler.

Shader "SF/FullscreenPass"
{
    VertexShader
    {
        #version 450

        // Outputs to fragment
        layout(location = 0) out vec2 outUV;

        void main()
        {
            // Generates a fullscreen triangle from gl_VertexIndex (0, 1, 2)
            outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
        }
    }

    FragmentShader
    {
        #version 450

        layout(location = 0) in  vec2 inUV;
        layout(location = 0) out vec4 outColor;

        layout(set = 0, binding = 1) uniform sampler2D colorSampler;

        void main()
        {
            outColor = texture(colorSampler, inUV);
        }
    }
}
