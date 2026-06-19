// Lit.shader : Standard opaque PBR mesh shader (forward, single-pass).
// Use for any opaque mesh that receives clustered lighting.
// Equivalent to Unity's "Lit" / Unreal's default lit material.
//
// Flat binding layout (single descriptor set, set=0):
//   bind 0  UBO   GpuFrameData
//   bind 1  SSBO  GpuLight[]
//   bind 2  SSBO  GpuClusterLightList[]
//   bind 3  SSBO  uint lightIndices[]
//   bind 4  sampler2D albedoMap
//   bind 5  sampler2D normalMap
//   bind 6  sampler2D pbrMap       (r=roughness, g=metallic, b=AO)
//   bind 7  sampler2D emissiveMap
// Push constants: model mat4, normalMatrix mat4, baseColor vec4,
//                 roughnessFactor, metallicFactor, aoFactor, emissiveFactor

Shader "SF/Lit"
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

        layout(set = 0, binding = 0) uniform FrameData {
            mat4 view; mat4 proj; mat4 viewProj;
            mat4 invView; mat4 invProj; mat4 invViewProj;
            vec4 cameraPos; vec4 cameraDir;
            vec2 screenSize; vec2 invScreenSize;
            float nearPlane; float farPlane; float time; float deltaTime;
            uint lightCount; uint frameIndex; vec2 _pad;
            vec4 sunDirIntensity; // .xyz=towardSun, .w=sunIntensity
        } frame;

        layout(push_constant) uniform PC {
            mat4  model;
            vec4  baseColor;
            float roughnessFactor; float metallicFactor; float aoFactor; float emissiveFactor;
        } push;

        void main() {
            vec4 wp4 = push.model * vec4(inPosition, 1.0);
            outWorldPos = wp4.xyz; outUV = inTexCoord;
            // Normal matrix (inverse-transpose) for non-uniform scale correctness
            mat3 normalMat = transpose(inverse(mat3(push.model)));
            vec3 N = normalize(normalMat * inNormal);
            // Tangent transformed by model matrix (NOT normal matrix) then
            // re-orthogonalised against world-space N so the TBN frame is
            // always orthonormal even with non-uniform scal    e.
            vec3 T = normalize(mat3(push.model) * inTangent);
            T = normalize(T - dot(T, N) * N);
            vec3 B = cross(N, T);   // right-handed bitangent
            outTBN = mat3(T, B, N);
            gl_Position = frame.viewProj * wp4;
        }
    }

    FragmentShader
    {
        #version 450

        layout(location = 0) in vec3 inWorldPos;
        layout(location = 1) in vec2 inUV;
        layout(location = 2) in mat3 inTBN;

        layout(location = 0) out vec4 outColor;

        layout(set = 0, binding = 0) uniform FrameData {
            mat4 view; mat4 proj; mat4 viewProj;
            mat4 invView; mat4 invProj; mat4 invViewProj;
            vec4 cameraPos; vec4 cameraDir;
            vec2 screenSize; vec2 invScreenSize;
            float nearPlane; float farPlane; float time; float deltaTime;
            uint lightCount; uint frameIndex; vec2 _pad;
            vec4 sunDirIntensity; // .xyz=towardSun, .w=sunIntensity
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

        layout(set = 0, binding = 4) uniform sampler2D albedoMap;
        layout(set = 0, binding = 5) uniform sampler2D normalMap;
        layout(set = 0, binding = 6) uniform sampler2D pbrMap;
        layout(set = 0, binding = 7) uniform sampler2D emissiveMap;

        layout(push_constant) uniform PC {
            mat4  model;
            vec4  baseColor;
            float roughnessFactor; float metallicFactor; float aoFactor; float emissiveFactor;
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

        // ACES filmic tonemap (Hill 2018 approximation)
        vec3 ACESFilm(vec3 x) {
            return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0.0,1.0);
        }

        uint ClusterIdx() {
            uvec2 tile = uvec2(gl_FragCoord.xy/(frame.screenSize/vec2(CLUSTER_X,CLUSTER_Y)));
            float vz = -(frame.view*vec4(inWorldPos,1.0)).z;
            uint  sl = uint(max(0.0,
                log(vz/frame.nearPlane)/log(frame.farPlane/frame.nearPlane)*float(CLUSTER_Z)));
            return tile.x+tile.y*CLUSTER_X+min(sl,uint(CLUSTER_Z-1))*CLUSTER_X*CLUSTER_Y;
        }

        vec3 EvalLight(Light l, vec3 P, vec3 N, vec3 V,
                       vec3 albedo, float rough, float metal, vec3 F0)
        {
            vec3 L; float atten=1.0;
            if (l.type==2u) {
                L = normalize(-l.direction);
            } else {
                vec3 d=l.position-P; float dist=length(d);
                if (dist>=l.radius) return vec3(0.0);
                L=d/dist;
                float t=dist/l.radius, w=max(0.0,1.0-t*t*t*t); w*=w;
                atten=w/max(dist*dist,1e-4);
                if (l.type==1u) {
                    float theta=dot(-L,normalize(l.direction));
                    atten*=clamp((theta-l.outerCone)/max(l.innerCone-l.outerCone,1e-4),0.0,1.0);
                }
            }
            float NdL=max(dot(N,L),0.0); if (NdL==0.0) return vec3(0.0);
            vec3  H=normalize(V+L);
            float NdH=max(dot(N,H),0.0), NdV=max(dot(N,V),0.0), HdV=max(dot(H,V),0.0);
            float D=DistGGX(NdH,rough), G=GeomSGGX(NdV,rough)*GeomSGGX(NdL,rough);
            vec3  F=Fresnel(HdV,F0);
            vec3  spec=(D*G*F)/max(4.0*NdV*NdL,1e-3);
            vec3  diff=(1.0-F)*(1.0-metal)*albedo/PI;
            return (diff+spec)*l.color*l.intensity*atten*NdL;
        }

        void main()
        {
            vec4  albedoS = texture(albedoMap, inUV) * push.baseColor;
            if (albedoS.a < 0.01) discard;

            vec3  tsN  = texture(normalMap, inUV).rgb * 2.0 - 1.0;
            vec3  N    = normalize(inTBN * tsN);
            vec4  pbr  = texture(pbrMap, inUV);
            float rough = max(pbr.r * push.roughnessFactor, 0.04);
            float metal = pbr.g * push.metallicFactor;
            float ao    = pbr.b * push.aoFactor;
            float emis  = texture(emissiveMap, inUV).r * push.emissiveFactor;

            vec3 albedo = albedoS.rgb;
            vec3 V      = normalize(frame.cameraPos.xyz - inWorldPos);
            vec3 F0     = mix(vec3(0.04), albedo, metal);

            // Sun visibility: continuously proportional to sun elevation.
            //   sunElevation > 0  → sun above horizon, full contribution scales with height
            //   sunElevation = 0  → horizon, soft twilight
            //   sunElevation < 0  → below horizon, fades through civil twilight to zero
            //
            // max(sunElevation, 0) gives a linear day ramp.
            // smoothstep(-0.1, 0.0, sunElevation) gives a soft horizon fade so the
            // cube doesn't snap off exactly at the mathematical horizon.
            // Together: full brightness at zenith, proportionally dimmer as sun lowers,
            // smoothly extinguished once it clears civil twilight.
            float sunElevation  = frame.sunDirIntensity.y;  // Y of toward-sun unit vec
            float horizonFade   = smoothstep(-0.10, 0.0, sunElevation); // soft horizon cutoff
            float sunVisibility = max(sunElevation, 0.0) * horizonFade; // 0→1 proportional

            // Ambient: hemisphere term at day, near-black floor at night.
            vec3 ambientDay   = mix(vec3(0.03), vec3(0.07), N.y * 0.5 + 0.5);
            vec3 ambientNight = vec3(0.001);
            vec3 Lo = mix(ambientNight, ambientDay, horizonFade) * albedo * ao;

            // Directional lights: scaled by sunVisibility so brightness is continuously
            // proportional to sun elevation : bright at noon, dim at golden hour, zero at night.
            // Point/spot lights are NOT scaled (artificial sources, sun-independent).
            for (uint i=0u; i<frame.lightCount; i++) {
                if (lights[i].type == 2u)
                    Lo += EvalLight(lights[i], inWorldPos, N, V, albedo, rough, metal, F0) * sunVisibility;
            }

            // Clustered light loop (point + spot only)
            uint cidx   = ClusterIdx();
            uint offset = lists[cidx].offset, count = lists[cidx].count;
            for (uint i=0u; i<count; i++) {
                if (lights[indices[offset+i]].type != 2u)  // skip directionals already done
                    Lo += EvalLight(lights[indices[offset+i]], inWorldPos, N, V,
                                    albedo, rough, metal, F0);
            }

            // Emissive
            Lo += albedo * emis * 4.0;

            // Tonemap + gamma
            Lo = ACESFilm(Lo);
            Lo = pow(Lo, vec3(1.0/2.2));
            outColor = vec4(Lo, 1.0);
        }
    }
}
