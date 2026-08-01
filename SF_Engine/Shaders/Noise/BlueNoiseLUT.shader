Shader "Clouds/BlueNoiseLUT"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
        layout(set = 0, binding = 0, r16f) uniform writeonly image2D outBlueNoise;
        
        #import "Noise/BlueNoise.si"

        void main()
        {
            ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
            ivec2 size  = imageSize(outBlueNoise);
            if (coord.x >= size.x || coord.y >= size.y) return;

            vec2 uv = vec2(coord) / vec2(size);
            float blueNoise = BlueNoiseErrorDistrib(
                uint(coord.x), 
                uint(coord.y), 
                0, 
                0u
            );

            imageStore(outBlueNoise, coord, vec4(blueNoise, 0.0, 0.0, 1.0));
        }
    }
}
