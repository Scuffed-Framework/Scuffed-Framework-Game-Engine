// CurlNoiseLUT.shader
// Bakes curl noise into a 3-D R8G8B8A8_UNORM texture used for cloud
// detail erosion (binding 1 in CloudRaymarch.shader).
//
// Curl noise = rot(grad(Perlin)) — divergence-free, great for wispy detail.
// We store (curlX, curlY, curlZ, magnitude) packed into RGBA.
//
// Dispatch:  ceil(size/4) x ceil(size/4) x ceil(size/4)

#import "Clouds/CloudNoise"

Shader "SF/Clouds/CurlNoiseLUT"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

        layout(set = 0, binding = 0, rgba8) uniform writeonly image3D outCurl;

        // Finite-difference curl of a Perlin potential field
        vec3 curlNoise(vec3 p, float freq)
        {
            const float e = 0.01;
            vec3 dx = vec3(e, 0.0, 0.0);
            vec3 dy = vec3(0.0, e, 0.0);
            vec3 dz = vec3(0.0, 0.0, e);

            // Potential: three independent scalar Perlin fields
            float px0 = gradientNoise(p - dx, freq),  px1 = gradientNoise(p + dx, freq);
            float py0 = gradientNoise(p - dy, freq),  py1 = gradientNoise(p + dy, freq);
            float pz0 = gradientNoise(p - dz, freq),  pz1 = gradientNoise(p + dz, freq);

            // Curl via central differences
            float dFy_dz = (pz1 - pz0) / (2.0 * e);
            float dFz_dy = (py1 - py0) / (2.0 * e);
            float dFz_dx = (px1 - px0) / (2.0 * e);
            float dFx_dz = (pz1 - pz0) / (2.0 * e);
            float dFx_dy = (py1 - py0) / (2.0 * e);
            float dFy_dx = (px1 - px0) / (2.0 * e);

            return vec3(dFy_dz - dFz_dy,
                        dFz_dx - dFx_dz,
                        dFx_dy - dFy_dx);
        }

        void main()
        {
            ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
            ivec3 size  = imageSize(outCurl);
            if (any(greaterThanEqual(coord, size)))
                return;

            vec3 uv = (vec3(coord) + 0.5) / vec3(size);

            // Two-octave curl
            vec3 c = curlNoise(uv * 4.0,  4.0) * 0.666
                   + curlNoise(uv * 8.0,  8.0) * 0.333;

            // Remap [-1,1] → [0,1] for storage
            vec3  packed = c * 0.5 + 0.5;
            float mag    = clamp(length(c) * 0.5, 0.0, 1.0);

            imageStore(outCurl, coord, vec4(packed, mag));
        }
    }
}
