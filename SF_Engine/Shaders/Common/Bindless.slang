#ifndef SHADER_BINDLESS_SLANG
#define SHADER_BINDLESS_SLANG

// Match C++ EBindingType layout at descriptor set 0
enum EBindingType
{
    BindlessStorageBuffer = 0,
    BindlessUniformBuffer = 1,
    BindlessSampledImage = 2,
    BindlessStorageImage = 3,
    BindlessSampler = 4,
    BindlessUniformTexelBuffer = 5,
    BindlessStorageTexelBuffer = 6,
    MAX
};

// -----------------------------------------------------------------------------
// Unbounded Descriptor Arrays (Set 0, Bindings 0-6)
// -----------------------------------------------------------------------------
[[vk::binding((int)EBindingType::BindlessStorageBuffer, 0)]] 
ByteAddressBuffer g_BindlessStorageBuffers[];

[[vk::binding((int)EBindingType::BindlessStorageBuffer, 0)]] 
RWByteAddressBuffer g_BindlessRWStorageBuffers[];

[[vk::binding((int)EBindingType::BindlessSampledImage, 0)]] 
Texture2D g_BindlessTextures2D[];

[[vk::binding((int)EBindingType::BindlessStorageImage, 0)]] 
RWTexture2D<float4> g_BindlessRWTextures2DFloat4[];

[[vk::binding((int)EBindingType::BindlessStorageImage, 0)]] 
RWTexture2D<uint4> g_BindlessRWTextures2DUint4[];

[[vk::binding((int)EBindingType::BindlessSampler, 0)]] 
SamplerState g_BindlessSamplers[];

/// Perform atomic addition on a uint in a storage buffer
uint interlockedAddUint(uint bufferId, uint count = 1, uint byteOffset = 0)
{
    uint originalValue;
    g_BindlessRWStorageBuffers[bufferId].InterlockedAdd(byteOffset, count, originalValue);
    return originalValue;
}

// Generic Texture Operations & Overloads
float4 loadTexture2D(uint imageId, uint2 coord)
{
    return g_BindlessTextures2D[imageId].Load(int3(coord, 0));
}

float4 sampleTexture2D(uint imageId, SamplerState samplerState, float2 uv, float lod = 0.0)
{
    return g_BindlessTextures2D[imageId].SampleLevel(samplerState, uv, lod);
}

float4 sampleTexture2D(uint imageId, uint samplerId, float2 uv, float lod = 0.0)
{
    SamplerState sam = g_BindlessSamplers[samplerId];
    return g_BindlessTextures2D[imageId].SampleLevel(sam, uv, lod);
}

void storeRWTexture2D(uint imageId, uint2 coord, float4 value)
{
    g_BindlessRWTextures2DFloat4[imageId][coord] = value;
}

void storeRWTexture2D(uint imageId, uint2 coord, uint4 value)
{
    g_BindlessRWTextures2DUint4[imageId][coord] = value;
}

// Helper for PBR BRDF LUT sampling
float2 sampleBRDFLut(uint brdfLutIndex, uint samplerIndex, float NoV, float roughness)
{
    float2 uv = saturate(float2(NoV, roughness));
    return sampleTexture2D(brdfLutIndex, samplerIndex, uv, 0.0).xy;
}

#endif // SHADER_BINDLESS_SLANG