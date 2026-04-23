// ShadowLUT.shader
// Pre-integrates cloud self-shadow transmittance into a 2-D R16_SFLOAT LUT.
// The axes encode:
//   x  = normalised cloud depth from the sun (0 = cloud top, 1 = cloud bot)
//   y  = cloud density parameter (0 = thin, 1 = thick)
//
// At runtime, CloudRaymarch.shader performs a short secondary march instead of
// this LUT for correctness.  This LUT is reserved for a future optimisation
// path (e.g. god-ray integration) or as a fallback on low-end hardware.
//
// Dispatch:  ceil(width/8) x ceil(height/8) x 1

Shader "SF/Clouds/ShadowLUT"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

        layout(set = 0, binding = 0, r16f) uniform writeonly image2D outShadow;

        void main()
        {
            ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
            ivec2 size  = imageSize(outShadow);
            if (any(greaterThanEqual(coord, size)))
                return;

            vec2 uv = (vec2(coord) + 0.5) / vec2(size);

            // uv.x = depth fraction (0=top, 1=bottom)
            // uv.y = density scale
            float depth   = uv.x;
            float density = uv.y * 0.15;  // max optical depth ~= 0.15 per unit

            // Beer-Lambert transmittance integrated along depth
            float od = density * depth;
            float T  = exp(-od);

            imageStore(outShadow, coord, vec4(T, 0.0, 0.0, 1.0));
        }
    }
}
