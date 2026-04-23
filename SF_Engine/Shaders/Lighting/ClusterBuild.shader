// ClusterBuild.shader : compute: build view-space cluster AABBs
// Dispatched once on startup / resize: CLUSTER_X * CLUSTER_Y * CLUSTER_Z threads

Shader "SF/Lighting/ClusterBuild"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

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

        struct Cluster { vec4 minAABB; vec4 maxAABB; };
        layout(set = 0, binding = 1) buffer ClusterBuffer { Cluster clusters[]; };

        #define CLUSTER_X 16
        #define CLUSTER_Y 9
        #define CLUSTER_Z 24

        // Unproject a screen-space point to view-space
        vec4 Screen2View(vec4 screen)
        {
            vec2 ndc = screen.xy * frame.invScreenSize * 2.0 - 1.0;
            vec4 view = frame.invProj * vec4(ndc, screen.z, screen.w);
            return view / view.w;
        }

        vec3 LineIntersectZPlane(vec3 a, vec3 b, float z)
        {
            vec3 d = b - a;
            float t = (z - a.z) / d.z;
            return a + t * d;
        }

        void main()
        {
            uvec3 id = gl_WorkGroupID;
            uint idx = id.x + id.y * CLUSTER_X + id.z * CLUSTER_X * CLUSTER_Y;

            // Tile size in screen pixels
            vec2 tileSize = frame.screenSize / vec2(CLUSTER_X, CLUSTER_Y);

            // Screen-space corners of this tile
            vec4 ss_min = vec4(vec2(id.xy)       * tileSize, -1.0, 1.0);
            vec4 ss_max = vec4(vec2(id.xy + 1u)  * tileSize, -1.0, 1.0);

            // View-space near/far for this depth slice (exponential distribution)
            float near = frame.nearPlane;
            float far  = frame.farPlane;
            float sliceNear = -near * pow(far / near, float(id.z)       / float(CLUSTER_Z));
            float sliceFar  = -near * pow(far / near, float(id.z + 1u)  / float(CLUSTER_Z));

            // Project tile corners to view space at near/far planes
            vec3 eye = vec3(0.0);
            vec3 minNear = Screen2View(ss_min).xyz;
            vec3 maxNear = Screen2View(ss_max).xyz;

            vec3 p000 = LineIntersectZPlane(eye, minNear, sliceNear);
            vec3 p001 = LineIntersectZPlane(eye, minNear, sliceFar);
            vec3 p010 = LineIntersectZPlane(eye, vec3(ss_min.x, ss_max.y, 0), sliceNear);
            vec3 p011 = LineIntersectZPlane(eye, vec3(ss_min.x, ss_max.y, 0), sliceFar);
            vec3 p100 = LineIntersectZPlane(eye, vec3(ss_max.x, ss_min.y, 0), sliceNear);
            vec3 p101 = LineIntersectZPlane(eye, vec3(ss_max.x, ss_min.y, 0), sliceFar);
            vec3 p110 = LineIntersectZPlane(eye, maxNear, sliceNear);
            vec3 p111 = LineIntersectZPlane(eye, maxNear, sliceFar);

            vec3 minV = min(min(min(p000,p001),min(p010,p011)),min(min(p100,p101),min(p110,p111)));
            vec3 maxV = max(max(max(p000,p001),max(p010,p011)),max(max(p100,p101),max(p110,p111)));

            clusters[idx].minAABB = vec4(minV, 0.0);
            clusters[idx].maxAABB = vec4(maxV, 0.0);
        }
    }
}
