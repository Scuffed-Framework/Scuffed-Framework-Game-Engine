// CoverageLUT.shader
// Bakes a top-down cloud coverage map into a 2-D R8_UNORM texture.
// The coverage controls how much of the sky is covered with clouds.
// Values close to 1 = overcast, values close to 0 = clear sky.
//
// This is a procedural default.  Artists may replace this with a hand-painted
// or weather-simulation texture at runtime.
//
// Dispatch:  ceil(width/8) x ceil(height/8) x 1

#import "Clouds/CloudNoise"

Shader "SF/Clouds/CoverageLUT"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

        layout(set = 0, binding = 0, r8) uniform writeonly image2D outCoverage;

        void main()
        {
            ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
            ivec2 size  = imageSize(outCoverage);
            if (any(greaterThanEqual(coord, size)))
                return;

            vec2 uv = (vec2(coord) + 0.5) / vec2(size);

            // Low-frequency Perlin to define large-scale weather patterns
            float cov  = gradientNoise(vec3(uv * 4.0, 0.0), 4.0) * 0.50;
                  cov += gradientNoise(vec3(uv * 8.0, 0.3), 8.0) * 0.30;
                  cov += gradientNoise(vec3(uv *16.0, 0.7),16.0) * 0.15;

            // Remap so partial coverage is most common
            cov = cov * 0.5 + 0.5;                  // [-1,1] → [0,1]
            cov = smoothstep(0.85, 1.00, cov);

            imageStore(outCoverage, coord, vec4(cov, 0.0, 0.0, 1.0));
        }
    }
}
