[[vk::binding(0, 1)]]
[[vk::image_format("rgba16f")]]
RWTexture2D<float4> resultColor;

[[vk::binding(1, 1)]]
[[vk::image_format("r32f")]]
RWTexture2D<float> resultDepth;

[[vk::binding(2, 1)]]
Texture2D<float4> samplerColor;

[[vk::binding(3, 1)]]
Texture2D<float> samplerDepth;

[numthreads(8, 8, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    int2 dimensions;
    resultColor.GetDimensions(dimensions.x, dimensions.y);
    int2 screenCoordinates = int2(globalThreadID.xy);
    
    if (screenCoordinates.x >= dimensions.x || screenCoordinates.y >= dimensions.y) 
        return;
    
    resultColor[screenCoordinates] = samplerColor.Load(int3(screenCoordinates, 0));
    resultDepth[screenCoordinates] = samplerDepth.Load(int3(screenCoordinates, 0)).r;
}