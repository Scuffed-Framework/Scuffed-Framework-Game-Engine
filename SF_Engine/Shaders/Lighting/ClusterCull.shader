// ClusterCull.slang : compute: assign lights to clusters
// One thread per cluster, tests sphere vs AABB for point/spot lights.
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0  ConstantBuffer     FrameData
//   binding 1  StructuredBuffer<Light>        (read-only)
//   binding 2  StructuredBuffer<Cluster>      (read-only)
//   binding 3  RWStructuredBuffer<ClusterList>
//   binding 4  RWStructuredBuffer<uint>       lightIndices

struct FrameData
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4x4 invView;
    float4x4 invProj;
    float4x4 invViewProj;
    float4 cameraPos;
    float4 cameraDir;
    float2 screenSize;
    float2 invScreenSize;
    float nearPlane;
    float farPlane;
    float time;
    float deltaTime;
    uint  lightCount;
    uint  frameIndex;
    float2 _pad;
};

[[vk::binding(0, 0)]]
ConstantBuffer<FrameData> frame;

struct Light
{
    float3 position;   float radius;
    float3 color;      float intensity;
    float3 direction;  float innerCone;
    float  outerCone;  uint  type;
    float  castShadow; float _pad;
};

[[vk::binding(1, 0)]] StructuredBuffer<Light> lights;

struct Cluster { float4 minAABB; float4 maxAABB; };
[[vk::binding(2, 0)]] StructuredBuffer<Cluster> clusters;

struct ClusterList { uint offset; uint count; };
[[vk::binding(3, 0)]] RWStructuredBuffer<ClusterList> lightLists;

[[vk::binding(4, 0)]] RWStructuredBuffer<uint> lightIndices;

#define CLUSTER_COUNT   (16 * 9 * 24)
#define MAX_PER_CLUSTER 128

// Sphere vs AABB intersection (view space)
bool sphereAABB(float3 centre, float radius, float3 aabbMin, float3 aabbMax)
{
    float3 closest = clamp(centre, aabbMin, aabbMax);
    float dist2 = dot(centre - closest, centre - closest);
    return dist2 <= radius * radius;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint clusterIdx = dispatchThreadID.x;
    if (clusterIdx >= CLUSTER_COUNT) return;

    float3 cMin = clusters[clusterIdx].minAABB.xyz;
    float3 cMax = clusters[clusterIdx].maxAABB.xyz;

    // Accumulate light indices into a local array first
    uint localIndices[MAX_PER_CLUSTER];
    uint localCount = 0u;

    for (uint li = 0u; li < frame.lightCount && localCount < MAX_PER_CLUSTER; li++)
    {
        Light l = lights[li];

        if (l.type == 2u)
        {
            // Directional : affects every cluster
            localIndices[localCount++] = li;
            continue;
        }

        // Transform light position to view space
        float3 viewPos = mul(frame.view, float4(l.position, 1.0)).xyz;

        if (sphereAABB(viewPos, l.radius, cMin, cMax))
            localIndices[localCount++] = li;
    }

    // Write into global index buffer
    uint base = clusterIdx * MAX_PER_CLUSTER;
    for (uint i = 0u; i < localCount; i++)
        lightIndices[base + i] = localIndices[i];

    lightLists[clusterIdx].offset = base;
    lightLists[clusterIdx].count  = localCount;
}
