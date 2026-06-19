// AlligatorNoiseLUT.shader
// Bakes the Nubis Cubed 4-channel voxel cloud detail noise LUT into a
// 128x128x128 RGBA8_UNORM 3-D texture.
//
// Channel layout matches Andrew Schneider, "Nubis Cubed" p.96 exactly:
//
//   R: Low  Freq Curly-Alligator  (wispy detail, low  frequency)
//   G: High Freq Curly-Alligator  (wispy detail, high frequency)
//   B: Low  Freq Alligator        (billowy detail, inverted, low  freq)
//   A: High Freq Alligator        (billowy detail, inverted, high freq)
//
// Quote (p.95):
//   "For wispy details ... we started with inverted alligator noise,
//    which creates nice web-like shapes, and then distorted it using
//    curl noise. We call this Curly-Alligator noise."
//
//   "Alligator [is used] ... [it] looks very much like our layered
//    Worley noise but with more appropriate cloud-like lacunarity."
//
// Usage in density sampler (p.97+):
//   R/G → noise_composite for wispy zones   (detail_type → 0)
//   B/A → noise_composite for billowy zones (detail_type → 1)
//   Blend: noise_composite = lerp(curlyAlligator, alligator, detail_type)
//
// Dispatch: ceil(128/4) x ceil(128/4) x ceil(128/4) = 32 x 32 x 32
//
// References:
//   Andrew Schneider, "Nubis Cubed: Methods and Madness to Model and
//   Render Immersive Real-Time Voxel-Based Clouds", SIGGRAPH 2023,
//   Advances in Real-Time Rendering in Games Course, pp.95-96.
//   https://www.sidefx.com/docs/hdk/alligator_2alligator_8_c-example.html

Shader "SF/Clouds/AlligatorNoiseLUT"
{
    ComputeShader
    {
        #version 450

        // PerlinWorleyNoise.si must come first — AlligatorNoise.si
        // depends on hash33(), gradientNoise(), remap(), f_remap(),
        // and perlinfbm() defined there.
        #import "Noise/PerlinWorleyNoise.si"
        #import "Noise/AlligatorNoise.si"

        layout(local_size_x = 4, local_size_y = 4, local_size_z = 4) in;

        layout(set = 0, binding = 0, rgba8) uniform writeonly image3D outNoise;

        // Curl distortion strength at the base frequency.
        // Halved each octave inside curlyAlligatorFBM.
        // 0.3 gives subtle web distortion without aliasing the 128^3 grid.
        const float kCurlStrength = 0.3;

        void main()
        {
            ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
            ivec3 size  = imageSize(outNoise);

            if (any(greaterThanEqual(coord, size)))
                return;

            vec3 uvw = (vec3(coord) + 0.5) / vec3(size);

            // R: low-frequency curly-alligator (large wispy web shapes)
            float ca_lo = curlyAlligatorFBM(uvw, 4.0, kCurlStrength);

            // G: high-frequency curly-alligator (fine wispy wisps)
            float ca_hi = curlyAlligatorFBM(uvw, 8.0, kCurlStrength * 0.6);

            // B: low-frequency alligator (large billows, invertCloud=true)
            float al_lo = alligatorFBM(uvw, 4.0, true);

            // A: high-frequency alligator (fine billows, invertCloud=true)
            float al_hi = alligatorFBM(uvw, 8.0, true);

            imageStore(outNoise, coord, vec4(ca_lo, ca_hi, al_lo, al_hi));
        }
    }
}
