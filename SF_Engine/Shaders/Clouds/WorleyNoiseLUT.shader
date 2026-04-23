// WorleyNoiseLUT.shader
// Bakes a tileable 4-octave Worley FBM into a 3-D R8_UNORM texture.
// Used as the cloud base-shape noise (binding 0 in CloudRaymarch.shader).
//
// Dispatch:  ceil(size/4) x ceil(size/4) x ceil(size/4)

#import "Clouds/CloudNoise"

Shader "SF/Clouds/WorleyNoiseLUT"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

        layout(set = 0, binding = 0, r8) uniform writeonly image3D outNoise;

        void main()
        {
            ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
            ivec3 size  = imageSize(outNoise);
            if (any(greaterThanEqual(coord, size)))
                return;

            vec3 uv = (vec3(coord) + 0.5) / vec3(size);

            // Four-octave tileable Worley FBM
            float n = worleyFbm(uv, 4.0)  * 0.50
                    + worleyFbm(uv, 8.0)  * 0.25
                    + worleyFbm(uv, 16.0) * 0.12
                    + worleyFbm(uv, 32.0) * 0.06;
            n = clamp(n, 0.0, 1.0);

            imageStore(outNoise, coord, vec4(n, 0.0, 0.0, 1.0));
        }
    }
}
