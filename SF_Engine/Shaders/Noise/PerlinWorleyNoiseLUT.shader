// WorleyNoiseLUT.shader
// Bakes a tileable 4-octave Worley FBM into a 3-D RGBA8_UNORM texture.
// Used as the cloud base-shape noise (binding 0 in CloudRaymarch.shader).
//
// Dispatch: ceil(size/4) x ceil(size/4) x ceil(size/4)

/*
R: shape noise (Worley FBM, freq 4.0)
G: detail noise medium (Worley FBM, freq 8.0)
B: detail noise small (Worley FBM, freq 16.0)
A: coverage/weather (billowy Perlin FBM)
*/

Shader "SF/Clouds/PerlinWorleyNoiseLUT"
{
    ComputeShader
    {
        #version 450
        #import "Noise/PerlinWorleyNoise.si"
        layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

        // Changed from r8 to rgba8 to support four channels
        layout(set = 0, binding = 0, rgba8) uniform writeonly image3D outNoise;

        void main()
        {
            ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
            ivec3 size  = imageSize(outNoise);

            if (any(greaterThanEqual(coord, size)))
                return;

            vec3 uvw = (vec3(coord) + 0.5) / vec3(size);
            float z = uvw.z;
            float freq = 4.0;

            float w0 = PerlinWorleyFBM(vec3(uvw.xy, z), freq * .75);      // shape noise
            float w1 = PerlinWorleyFBM(vec3(uvw.xy, z), freq * 2.0); // medium detail
            float w2 = PerlinWorleyFBM(vec3(uvw.xy, z), freq * 4.0); // small detail
            float w3 = remap(PerlinWorleyFBM(vec3(uvw.xy, z), freq * .5), -1., 1., 0., 1.);
            w3 = remap(w3, 0.85, 1.0, 0.0, 1.0); // fake cloud coverage

            // Pack into RGBA channels
            vec4 pw = vec4(w0, w1, w2, w3);

            imageStore(outNoise, coord, pw);
        }
    }
}