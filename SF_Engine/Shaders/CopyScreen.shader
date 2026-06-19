Shader "CopyScreen"
{
    ComputeShader
    {
        #version 450

        layout (local_size_x = 8, local_size_y = 8) in;
        layout (set = 1, binding = 0, rgba16f)      uniform image2D resultColor;
        layout (set = 1, binding = 1, r32f)         uniform image2D resultDepth;
        layout (set = 1, binding = 2)               uniform sampler2D samplerDepth;
        layout (set = 1, binding = 3)               uniform sampler2D samplerColor;


        void main()
        {
            ivec2 dimensions = imageSize(resultColor);
            ivec2 screenCoordinates = ivec2(gl_GlobalInvocationID.xy); // It is a uvec3
            if (screenCoordinates.x >= dimensions.x || screenCoordinates.y >= dimensions.y) return;
            
            imageStore(resultColor, screenCoordinates, texelFetch(samplerColor, screenCoordinates, 0));
            imageStore(resultDepth, screenCoordinates, texelFetch(samplerDepth, screenCoordinates, 0));
        }
    }
}