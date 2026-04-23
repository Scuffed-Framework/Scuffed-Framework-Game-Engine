// ForwardTransparent.shader
// Flat binding layout (single descriptor set, set=0):
//   bind 0  UBO   GpuFrameData
//   bind 1  SSBO  GpuLight[]
//   bind 2  SSBO  GpuClusterLightList[]
//   bind 3  SSBO  uint lightIndices[]
//   bind 4  sampler2D sceneHDR
//   bind 5  sampler2D sceneDepth
// Push constants: model mat4, normalMatrix mat4, baseColor vec4,
//                 roughness, metallic, ior, refractionStrength

Shader "SF/Lighting/ForwardTransparent"
{
    VertexShader
    {
        #version 450

        layout(location = 0) in vec3 inPosition;
        layout(location = 1) in vec3 inNormal;
        layout(location = 2) in vec2 inTexCoord;
        layout(location = 3) in vec3 inTangent;

        layout(location = 0) out vec3 outWorldPos;
        layout(location = 1) out vec2 outUV;
        layout(location = 2) out mat3 outTBN;
        layout(location = 5) out vec4 outClipPos;

        layout(set = 0, binding = 0) uniform FrameData {
            mat4 view; mat4 proj; mat4 viewProj;
            mat4 invView; mat4 invProj; mat4 invViewProj;
            vec4 cameraPos; vec4 cameraDir;
            vec2 screenSize; vec2 invScreenSize;
            float nearPlane; float farPlane; float time; float deltaTime;
            uint lightCount; uint frameIndex; vec2 _pad;
        } frame;

        layout(push_constant) uniform PC {
            mat4  model;
            vec4  baseColor; float roughness; float metallic; float ior; float refractionStrength;
        } push;

        void main() {
            vec4 wp4 = push.model * vec4(inPosition, 1.0);
            outWorldPos = wp4.xyz; outUV = inTexCoord;
            mat3 normalMat = transpose(inverse(mat3(push.model)));
            vec3 N = normalize(normalMat * inNormal);
            vec3 T = normalize(normalMat * inTangent);
            T = normalize(T - dot(T,N)*N);
            outTBN = mat3(T, cross(N,T), N);
            gl_Position = frame.viewProj * wp4;
            outClipPos  = gl_Position;
        }
    }

    FragmentShader
    {
        #version 450

        layout(location = 0) in vec3 inWorldPos;
        layout(location = 1) in vec2 inUV;
        layout(location = 2) in mat3 inTBN;
        layout(location = 5) in vec4 inClipPos;

        layout(location = 0) out vec4 outColor;

        layout(set = 0, binding = 0) uniform FrameData {
            mat4 view; mat4 proj; mat4 viewProj;
            mat4 invView; mat4 invProj; mat4 invViewProj;
            vec4 cameraPos; vec4 cameraDir;
            vec2 screenSize; vec2 invScreenSize;
            float nearPlane; float farPlane; float time; float deltaTime;
            uint lightCount; uint frameIndex; vec2 _pad;
        } frame;

        struct Light {
            vec3 position; float radius; vec3 color; float intensity;
            vec3 direction; float innerCone; float outerCone; uint type; float castShadow; float _pad;
        };
        layout(set = 0, binding = 1) readonly buffer LightBuf     { Light lights[]; };
        struct ClusterList { uint offset; uint count; };
        layout(set = 0, binding = 2) readonly buffer LightListBuf { ClusterList lists[]; };
        layout(set = 0, binding = 3) readonly buffer LightIdxBuf  { uint indices[]; };

        layout(set = 0, binding = 4) uniform sampler2D sceneHDR;
        layout(set = 0, binding = 5) uniform sampler2D sceneDepth;

        layout(push_constant) uniform PC {
            mat4  model;
            vec4  baseColor;
            float roughness; float metallic; float ior; float refractionStrength;
        } push;

        #define PI        3.14159265359
        #define CLUSTER_X 16
        #define CLUSTER_Y 9
        #define CLUSTER_Z 24

        float DistGGX(float NdH, float a) {
            float a2=a*a*a*a; float d=NdH*NdH*(a2-1.0)+1.0; return a2/(PI*d*d);
        }
        float GeomSGGX(float NdX, float a) {
            float k=(a+1.0)*(a+1.0)/8.0; return NdX/(NdX*(1.0-k)+k);
        }
        vec3 Fresnel(float cos, vec3 F0) {
            return F0+(1.0-F0)*pow(clamp(1.0-cos,0.0,1.0),5.0);
        }
        uint ClusterIdx() {
            uvec2 tile = uvec2(gl_FragCoord.xy/(frame.screenSize/vec2(CLUSTER_X,CLUSTER_Y)));
            float vz = -(frame.view*vec4(inWorldPos,1.0)).z;
            uint sl = uint(max(0.0,log(vz/frame.nearPlane)/log(frame.farPlane/frame.nearPlane)*float(CLUSTER_Z)));
            return tile.x+tile.y*CLUSTER_X+min(sl,uint(CLUSTER_Z-1))*CLUSTER_X*CLUSTER_Y;
        }

        void main()
        {
            vec3  N  = normalize(inTBN * vec3(0.0, 0.0, 1.0));
            vec3  V  = normalize(frame.cameraPos.xyz - inWorldPos);
            float r0 = (push.ior-1.0)/(push.ior+1.0); r0 *= r0;
            vec3  F0 = mix(vec3(r0), push.baseColor.rgb, push.metallic);
            float fresnel = Fresnel(max(dot(N,V),0.0), F0).r;

            // Screen-space refraction
            vec2 scrUV    = (inClipPos.xy/inClipPos.w)*0.5+0.5;
            vec2 refractUV = clamp(scrUV + N.xy*push.refractionStrength*(1.0-fresnel),
                                   vec2(0.001), vec2(0.999));
            vec3 behind    = texture(sceneHDR, refractUV).rgb;

            // Clustered specular highlights
            vec3 spec = vec3(0.0);
            uint cidx = ClusterIdx();
            for (uint i=0u; i<lists[cidx].count; i++) {
                Light l = lights[indices[lists[cidx].offset+i]];
                vec3 L = (l.type==2u) ? normalize(l.direction) : normalize(l.position-inWorldPos);
                vec3 H = normalize(V+L);
                float NdL=max(dot(N,L),0.0); if (NdL==0.0) continue;
                float NdH=max(dot(N,H),0.0), NdV=max(dot(N,V),0.0), HdV=max(dot(H,V),0.0);
                float D=DistGGX(NdH,push.roughness), G=GeomSGGX(NdV,push.roughness)*GeomSGGX(NdL,push.roughness);
                vec3  F=Fresnel(HdV,F0);
                float atten=1.0;
                if (l.type!=2u) {
                    float dist=length(l.position-inWorldPos), t=dist/l.radius;
                    float w=max(0.0,1.0-t*t*t*t); w*=w; atten=w/max(dist*dist,1e-4);
                }
                spec += (D*G*F)/(4.0*NdV*NdL+1e-3)*l.color*l.intensity*atten*NdL;
            }

            vec3  color = mix(behind, push.baseColor.rgb, push.baseColor.a*0.3) + spec;
            float alpha = clamp(fresnel + push.baseColor.a*0.5, 0.0, 1.0);
            outColor = vec4(color*alpha, alpha);  // premultiplied
        }
    }
}
