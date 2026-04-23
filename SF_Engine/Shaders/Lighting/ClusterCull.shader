// ClusterCull.shader : compute: assign lights to clusters
// One thread per cluster, tests sphere vs AABB for point/spot lights.

Shader "SF/Lighting/ClusterCull"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 64) in;

        layout(set = 0, binding = 0) uniform FrameData {
            mat4 view;
            mat4 proj;
            mat4 viewProj;
            mat4 invView;
            mat4 invProj;
            mat4 invViewProj;
            vec4 cameraPos;
            vec4 cameraDir;
            vec2 screenSize;
            vec2 invScreenSize;
            float nearPlane;
            float farPlane;
            float time;
            float deltaTime;
            uint  lightCount;
            uint  frameIndex;
            vec2  _pad;
        } frame;

        struct Light {
            vec3  position;     float radius;
            vec3  color;        float intensity;
            vec3  direction;    float innerCone;
            float outerCone;    uint  type;
            float castShadow;   float _pad;
        };
        layout(set = 0, binding = 1) readonly buffer LightBuffer  { Light lights[]; };

        struct Cluster { vec4 minAABB; vec4 maxAABB; };
        layout(set = 0, binding = 2) readonly buffer ClusterBuffer { Cluster clusters[]; };

        struct ClusterList { uint offset; uint count; };
        layout(set = 0, binding = 3) buffer LightListBuffer { ClusterList lightLists[]; };

        layout(set = 0, binding = 4) buffer LightIndexBuffer { uint lightIndices[]; };

        #define CLUSTER_COUNT    (16 * 9 * 24)
        #define MAX_PER_CLUSTER  128

        // Sphere vs AABB intersection (view space)
        bool SphereAABB(vec3 centre, float radius, vec3 aabbMin, vec3 aabbMax)
        {
            vec3 closest = clamp(centre, aabbMin, aabbMax);
            float dist2  = dot(centre - closest, centre - closest);
            return dist2 <= radius * radius;
        }

        void main()
        {
            uint clusterIdx = gl_GlobalInvocationID.x;
            if (clusterIdx >= CLUSTER_COUNT) return;

            vec3 cMin = clusters[clusterIdx].minAABB.xyz;
            vec3 cMax = clusters[clusterIdx].maxAABB.xyz;

            // Accumulate light indices into a local array first
            uint localIndices[MAX_PER_CLUSTER];
            uint localCount = 0u;

            for (uint li = 0u; li < frame.lightCount && localCount < MAX_PER_CLUSTER; li++)
            {
                Light l = lights[li];

                if (l.type == 2u) {
                    // Directional : affects every cluster
                    localIndices[localCount++] = li;
                    continue;
                }

                // Transform light position to view space
                vec3 viewPos = (frame.view * vec4(l.position, 1.0)).xyz;

                if (SphereAABB(viewPos, l.radius, cMin, cMax))
                    localIndices[localCount++] = li;
            }

            // Write into global index buffer
            uint base = clusterIdx * MAX_PER_CLUSTER;
            for (uint i = 0u; i < localCount; i++)
                lightIndices[base + i] = localIndices[i];

            lightLists[clusterIdx].offset = base;
            lightLists[clusterIdx].count  = localCount;
        }
    }
}
