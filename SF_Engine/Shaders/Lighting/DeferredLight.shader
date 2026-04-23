// DeferredLight.shader
// Flat binding layout (single descriptor set, set=0):
//   bind 0  UBO   GpuFrameData
//   bind 1  SSBO  GpuLight[]
//   bind 2  SSBO  GpuClusterLightList[]
//   bind 3  SSBO  uint lightIndices[]
//   bind 4  sampler2D gbufAlbedo
//   bind 5  sampler2D gbufNormal
//   bind 6  sampler2D gbufPBR
//   bind 7  sampler2D gbufDepth

Shader "SF/Lighting/DeferredLight"
{
    VertexShader
    {
        #version 450
        layout(location = 0) out vec2 outUV;
        void main() {
            outUV       = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
        }
    }

    FragmentShader
    {
        #version 450

        layout(location = 0) in  vec2 inUV;
        layout(location = 0) out vec4 outHDR;

        layout(set = 0, binding = 0) uniform FrameData {
            mat4 view; mat4 proj; mat4 viewProj;
            mat4 invView; mat4 invProj; mat4 invViewProj;
            vec4 cameraPos; vec4 cameraDir;
            vec2 screenSize; vec2 invScreenSize;
            float nearPlane; float farPlane; float time; float deltaTime;
            uint lightCount; uint frameIndex; vec2 _pad;
        } frame;

        struct Light {
            vec3 position; float radius;
            vec3 color;    float intensity;
            vec3 direction; float innerCone;
            float outerCone; uint type; float castShadow; float _pad;
        };
        layout(set = 0, binding = 1) readonly buffer LightBuf     { Light lights[]; };
        struct ClusterList { uint offset; uint count; };
        layout(set = 0, binding = 2) readonly buffer LightListBuf { ClusterList lists[]; };
        layout(set = 0, binding = 3) readonly buffer LightIdxBuf  { uint indices[]; };

        layout(set = 0, binding = 4) uniform sampler2D gbufAlbedo;
        layout(set = 0, binding = 5) uniform sampler2D gbufNormal;
        layout(set = 0, binding = 6) uniform sampler2D gbufPBR;
        layout(set = 0, binding = 7) uniform sampler2D gbufDepth;

        #define PI        3.14159265359
        #define CLUSTER_X 16
        #define CLUSTER_Y 9
        #define CLUSTER_Z 24

        vec3 OctDecode(vec2 f) {
            vec3 n = vec3(f, 1.0 - abs(f.x) - abs(f.y));
            if (n.z < 0.0) n.xy = (1.0 - abs(n.yx)) * sign(n.xy);
            return normalize(n);
        }

        vec3 WorldPosFromDepth(float depth, vec2 uv) {
            vec4 ndc = vec4(uv * 2.0 - 1.0, depth, 1.0);
            vec4 wp  = frame.invViewProj * ndc;
            return wp.xyz / wp.w;
        }

        float DistGGX(float NdH, float a) {
            float a2 = a*a*a*a; float d = NdH*NdH*(a2-1.0)+1.0;
            return a2/(PI*d*d);
        }
        float GeomSGGX(float NdX, float a) {
            float k = (a+1.0)*(a+1.0)/8.0; return NdX/(NdX*(1.0-k)+k);
        }
        vec3 Fresnel(float cos, vec3 F0) {
            return F0+(1.0-F0)*pow(clamp(1.0-cos,0.0,1.0),5.0);
        }

        uint ClusterIdx(vec3 wp) {
            uvec2 tile  = uvec2(gl_FragCoord.xy /
                                (frame.screenSize / vec2(CLUSTER_X, CLUSTER_Y)));
            float viewZ = -(frame.view * vec4(wp, 1.0)).z;
            uint  slice = uint(max(0.0,
                log(viewZ/frame.nearPlane)/log(frame.farPlane/frame.nearPlane)*float(CLUSTER_Z)));
            return tile.x + tile.y*CLUSTER_X + min(slice,uint(CLUSTER_Z-1))*CLUSTER_X*CLUSTER_Y;
        }

        vec3 EvalLight(Light l, vec3 P, vec3 N, vec3 V,
                       vec3 albedo, float rough, float metal, vec3 F0)
        {
            vec3  L; float atten = 1.0;
            if (l.type == 2u) {
                L = normalize(l.direction);
            } else {
                vec3 d = l.position - P; float dist = length(d);
                if (dist >= l.radius) return vec3(0.0);
                L = d/dist;
                float t = dist/l.radius, w = max(0.0,1.0-t*t*t*t); w*=w;
                atten = w/max(dist*dist,1e-4);
                if (l.type == 1u) {
                    float theta = dot(-L, normalize(l.direction));
                    atten *= clamp((theta-l.outerCone)/max(l.innerCone-l.outerCone,1e-4),0.0,1.0);
                }
            }
            float NdL = max(dot(N,L),0.0); if (NdL==0.0) return vec3(0.0);
            vec3  H   = normalize(V+L);
            float NdH = max(dot(N,H),0.0), NdV = max(dot(N,V),0.0), HdV = max(dot(H,V),0.0);
            float D = DistGGX(NdH,rough), G = GeomSGGX(NdV,rough)*GeomSGGX(NdL,rough);
            vec3  F = Fresnel(HdV,F0);
            vec3  spec = (D*G*F)/max(4.0*NdV*NdL,1e-3);
            vec3  diff = (1.0-F)*(1.0-metal)*albedo/PI;
            return (diff+spec)*l.color*l.intensity*atten*NdL;
        }

        void main()
        {
            float depth = texture(gbufDepth, inUV).r;
            if (depth >= 1.0) { outHDR = vec4(0.0,0.0,0.0,1.0); return; }

            vec3  albedo = texture(gbufAlbedo, inUV).rgb;
            vec3  N      = OctDecode(texture(gbufNormal, inUV).rg);
            vec4  pbr    = texture(gbufPBR, inUV);
            float rough  = max(pbr.r, 0.04), metal = pbr.g, ao = pbr.b, emis = pbr.a;

            vec3  wp  = WorldPosFromDepth(depth, inUV);
            vec3  V   = normalize(frame.cameraPos.xyz - wp);
            vec3  F0  = mix(vec3(0.04), albedo, metal);

            vec3  Lo  = mix(vec3(0.03), vec3(0.07), N.y*0.5+0.5) * albedo * ao;

            uint cidx   = ClusterIdx(wp);
            uint offset = lists[cidx].offset, count = lists[cidx].count;
            for (uint i = 0u; i < count; i++)
                Lo += EvalLight(lights[indices[offset+i]], wp, N, V, albedo, rough, metal, F0);

            Lo += albedo * emis * 4.0;
            outHDR = vec4(Lo, 1.0);
        }
    }
}
