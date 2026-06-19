// BloomBlur.shader : Pass 2 of 3.
// 13-tap separable Gaussian blur (UE-style dual-pass).
// Run twice: first horizontal (direction=(1,0)), then vertical (direction=(0,1)).
//
// set=0  bind=0  sampler2D srcTex      (bright-pass result or previous blur result)
// set=0  bind=1  UBO BloomBlurUBO
//
// Gaussian weights for sigma~2.0, 13 taps, normalised.

Shader "SF/PostProcess/BloomBlur"
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

        layout(set = 0, binding = 0) uniform sampler2D srcTex;

        layout(set = 0, binding = 1) uniform BloomBlurUBO
        {
            vec2  texelSize;    // 1.0 / textureSize(srcTex)
            vec2  direction;    // (1,0) for horizontal, (0,1) for vertical
            float spread;       // blur radius scale (1.0 = standard, 2.0 = wider)
            vec3  _pad;
        } u;

        // 13-tap Gaussian weights (sigma ~2.0, summing to 1.0)
        const int   TAPS    = 13;
        const float W[13]   = float[](
            0.00598, 0.02132, 0.05988, 0.13209, 0.22821,
            0.31062,
            0.22821, 0.13209, 0.05988, 0.02132, 0.00598,
            0.0, 0.0   // padding to fixed array size : unused
        );
        const int HALF = 5; // offsets run -5..+5 (11 taps used with the centre)

        void main()
        {
            vec3 acc = vec3(0.0);
            for (int i = -HALF; i <= HALF; i++)
            {
                vec2 offset = u.direction * u.texelSize * float(i) * u.spread;
                acc += texture(srcTex, inUV + offset).rgb * W[i + HALF];
            }
            outColor = vec4(acc, 1.0);
        }
    }
}
