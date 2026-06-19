Shader "SF/Noise/Perlin"
{
    ComputeShader
    {
        #version 450
        #import "Noise/PerlinNoise.si"

        layout (local_size_x = 8, local_size_y = 8) in;

        layout(set = 0, binding = 0, r8) uniform writeonly image2D perlinNoiseTex;

        void main()
        {
            ivec2 uv = ivec2(gl_GlobalInvocationID.xy);

            // get texture size
            ivec2 size = imageSize(perlinNoiseTex);
            if (uv.x >= size.x || uv.y >= size.y)
                return;

            // Normalized coords
            vec2 p = vec2(uv) / vec2(size);

            // Turn 2D -> 3D Perlin input
            vec3 pos = vec3(p * 8.0, 0.0);

            float n = perlin(pos) * 0.5 + 0.5;  // remap to 0–1

            imageStore(perlinNoiseTex, uv, vec4(n, 0.0, 0.0, 1.0));
        }
    }
}