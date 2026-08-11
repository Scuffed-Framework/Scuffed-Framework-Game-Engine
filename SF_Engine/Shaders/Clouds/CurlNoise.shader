#include "Noise/PerlinNoise.si"

[[vk::binding(0, 0)]] [[vk::image_format("rgba8")]]
RWTexture2D<float4> outCurlNoise;

// Constants - adjust these as needed
static const float NOISE_SCALE = 4.0f;
static const float EPSILON = 1.0f / 128.0f;  // ~0.0078125

float3 potential(float3 p)
{
    // Three independent noise fields using different offsets as seeds
    return float3(
        perlin(p + float3(0.0, 0.0, 0.0)),
        perlin(p + float3(100.0, 100.0, 100.0)),
        perlin(p + float3(200.0, 200.0, 200.0))
    );
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    int2 res;
    outCurlNoise.GetDimensions(res.x, res.y);
    
    // Bounds check
    if (any(id.xy >= res)) return;

    // Map pixel to 3D position - FIXED
    float2 uv = (float2(id.xy) + 0.5) / float2(res);
    float3 p = float3(uv * NOISE_SCALE, 0.0);  // z=0 for 2D slice

    // Central differences for gradient
    float3 dx = (potential(p + float3(EPSILON, 0, 0)) - 
                 potential(p - float3(EPSILON, 0, 0))) / (2.0 * EPSILON);
    float3 dy = (potential(p + float3(0, EPSILON, 0)) - 
                 potential(p - float3(0, EPSILON, 0))) / (2.0 * EPSILON);
    float3 dz = (potential(p + float3(0, 0, EPSILON)) - 
                 potential(p - float3(0, 0, EPSILON))) / (2.0 * EPSILON);

    // Curl = ∇ × Ψ
    float3 curl = float3(
        dy.z - dz.y,  // ∂Ψz/∂y - ∂Ψy/∂z
        dz.x - dx.z,  // ∂Ψx/∂z - ∂Ψz/∂x
        dx.y - dy.x   // ∂Ψy/∂x - ∂Ψx/∂y
    );

    // Map from [-1,1] to [0,1] for display
    outCurlNoise[id.xy] = float4(curl * 0.5 + 0.5, 1.0);
}