// ClusterBuild.slang : compute: build view-space cluster AABBs
// Dispatched once on startup / resize: CLUSTER_X * CLUSTER_Y * CLUSTER_Z threads
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0  ConstantBuffer   FrameData
//   binding 1  RWStructuredBuffer<Cluster>

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

struct Cluster { float4 minAABB; float4 maxAABB; };

[[vk::binding(1, 0)]]
RWStructuredBuffer<Cluster> clusters;

#define CLUSTER_X 16
#define CLUSTER_Y 9
#define CLUSTER_Z 24

// Unproject a screen-space point to view-space
float4 screen2View(float4 screen)
{
    float2 ndc = screen.xy * frame.invScreenSize * 2.0 - 1.0;
    float4 view = mul(frame.invProj, float4(ndc, screen.z, screen.w));
    return view / view.w;
}

float3 lineIntersectZPlane(float3 a, float3 b, float z)
{
    float3 d = b - a;
    float t = (z - a.z) / d.z;
    return a + t * d;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 groupID: SV_GroupID)
{
    uint3 id = groupID;
    uint idx = id.x + id.y * CLUSTER_X + id.z * CLUSTER_X * CLUSTER_Y;

    float2 tileSize = frame.screenSize / float2(CLUSTER_X, CLUSTER_Y);

    float4 ss_topLeft = float4(float2(id.x, id.y) * tileSize, -1.0, 1.0);
    float4 ss_topRight = float4(float2(id.x + 1u, id.y) * tileSize, -1.0, 1.0);
    float4 ss_botLeft = float4(float2(id.x, id.y + 1u) * tileSize, -1.0, 1.0);
    float4 ss_botRight = float4(float2(id.x + 1u, id.y + 1u) * tileSize, -1.0, 1.0);

    float near = frame.nearPlane;
    float far = frame.farPlane;
    float sliceNear = -near * pow(far / near, float(id.z) / float(CLUSTER_Z));
    float sliceFar = -near * pow(far / near, float(id.z + 1u) / float(CLUSTER_Z));

    float3 eye = float3(0.0, 0.0, 0.0);

    // Unproject ALL FOUR corners through screen2View before intersecting
    float3 viewTopLeft = screen2View(ss_topLeft).xyz;
    float3 viewTopRight = screen2View(ss_topRight).xyz;
    float3 viewBotLeft = screen2View(ss_botLeft).xyz;
    float3 viewBotRight = screen2View(ss_botRight).xyz;

    float3 p000 = lineIntersectZPlane(eye, viewTopLeft, sliceNear);
    float3 p001 = lineIntersectZPlane(eye, viewTopLeft, sliceFar);
    float3 p010 = lineIntersectZPlane(eye, viewBotLeft, sliceNear);
    float3 p011 = lineIntersectZPlane(eye, viewBotLeft, sliceFar);
    float3 p100 = lineIntersectZPlane(eye, viewTopRight, sliceNear);
    float3 p101 = lineIntersectZPlane(eye, viewTopRight, sliceFar);
    float3 p110 = lineIntersectZPlane(eye, viewBotRight, sliceNear);
    float3 p111 = lineIntersectZPlane(eye, viewBotRight, sliceFar);

    float3 minV = min(min(min(p000, p001), min(p010, p011)), min(min(p100, p101), min(p110, p111)));
    float3 maxV = max(max(max(p000, p001), max(p010, p011)), max(max(p100, p101), max(p110, p111)));

    clusters[idx].minAABB = float4(minV, 0.0);
    clusters[idx].maxAABB = float4(maxV, 0.0);
}
