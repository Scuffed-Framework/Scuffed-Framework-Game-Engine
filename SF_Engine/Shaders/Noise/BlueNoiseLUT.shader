Shader "Clouds/BlueNoiseLUT"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

        layout(set = 0, binding = 0, r16f) uniform writeonly image2D outBlueNoise;

        // Interleaved gradient noise  excellent blue noise approximation,
        // no texture lookup needed, works per-pixel deterministically.
        // Based on Jimenez 2014 / Karis 2021.
        float interleavedGradientNoise(vec2 pixel)
        {
            vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
            return fract(magic.z * fract(dot(pixel, magic.xy)));
        }

        // Multi-scale hash for spatial variety across the texture
        float hash2(vec2 p)
        {
            p = fract(p * vec2(127.1, 311.7));
            p += dot(p, p + 19.19);
            return fract(p.x * p.y);
        }

        // Approximate void-and-cluster: layered IGN at different scales
        // to produce a low-discrepancy distribution that looks blue-noise-like.
        float blueNoiseApprox(vec2 uv, ivec2 size)
        {
            // Primary IGN signal
            float n0 = interleavedGradientNoise(uv * vec2(size));

            // Correction layer: subtract low-frequency content
            float lf = hash2(uv * 4.0) * 0.5
                     + hash2(uv * 2.0) * 0.3
                     + hash2(uv * 1.0) * 0.2;

            // High-pass: keep only high-frequency content
            float signal = fract(n0 - lf * 0.35 + 0.5);
            return signal;
        }

        void main()
        {
            ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
            ivec2 size  = imageSize(outBlueNoise);
            if (coord.x >= size.x || coord.y >= size.y) return;

            vec2 uv = vec2(coord) / vec2(size);
            float bn = blueNoiseApprox(uv, size);

            imageStore(outBlueNoise, coord, vec4(bn, 0.0, 0.0, 1.0));
        }
    }
}
