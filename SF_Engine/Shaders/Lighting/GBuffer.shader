// GBuffer.shader
// Flat binding layout (single descriptor set, set=0):
//   bind 0  UBO   GpuFrameData
//   bind 1  sampler2D albedoMap
//   bind 2  sampler2D normalMap
//   bind 3  sampler2D pbrMap       (r=roughness, g=metallic, b=AO)
//   bind 4  sampler2D emissiveMap
// Push constants: model mat4, normalMatrix mat4, baseColor vec4,
//                 roughnessFactor, metallicFactor, aoFactor, emissiveFactor

Shader "SF/Lighting/GBuffer"
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
        } frame;

        layout(push_constant) uniform PC {
            mat4  model;
            vec4  baseColor;
            float roughnessFactor; float metallicFactor; float aoFactor; float emissiveFactor;
        } push;

        void main() {
            vec4 wp4 = push.model * vec4(inPosition, 1.0);
            outWorldPos = wp4.xyz; outUV = inTexCoord;
            mat3 normalMat = transpose(inverse(mat3(push.model)));
            vec3 N = normalize(normalMat * inNormal);
            vec3 T = normalize(normalMat * inTangent);
            T = normalize(T - dot(T,N)*N);
            outTBN = mat3(T, cross(T,N), N);
            gl_Position = frame.viewProj * wp4;
        }
    }

    FragmentShader
    {
        #version 450

        layout(location = 0) in vec3 inWorldPos;
        layout(location = 1) in vec2 inUV;
        layout(location = 2) in mat3 inTBN;

        layout(location = 0) out vec4 outAlbedo;   // rgb=albedo a=opacity
        layout(location = 1) out vec2 outNormal;   // oct-encoded
        layout(location = 2) out vec4 outPBR;      // r=rough g=metal b=ao a=emissive

        layout(set = 0, binding = 1) uniform sampler2D albedoMap;
        layout(set = 0, binding = 2) uniform sampler2D normalMap;
        layout(set = 0, binding = 3) uniform sampler2D pbrMap;
        layout(set = 0, binding = 4) uniform sampler2D emissiveMap;

        layout(push_constant) uniform PC {
            mat4  model;
            vec4  baseColor;
            float roughnessFactor; float metallicFactor; float aoFactor; float emissiveFactor;
        } push;

        vec2 OctEncode(vec3 n) {
            n /= (abs(n.x)+abs(n.y)+abs(n.z));
            if (n.z < 0.0) {
                vec2 s = sign(n.xy);
                n.xy = (1.0-abs(n.yx))*mix(vec2(-1.0),vec2(1.0),step(0.0,s));
            }
            return n.xy;
        }

        void main() {
            vec4 albedo = texture(albedoMap, inUV) * push.baseColor;
            if (albedo.a < 0.01) discard;

            vec3 tsN    = texture(normalMap, inUV).rgb * 2.0 - 1.0;
            vec3 worldN = normalize(inTBN * tsN);
            vec4 pbr    = texture(pbrMap, inUV);

            outAlbedo = vec4(albedo.rgb, albedo.a);
            outNormal = OctEncode(worldN);
            outPBR    = vec4(pbr.r * push.roughnessFactor,
                             pbr.g * push.metallicFactor,
                             pbr.b * push.aoFactor,
                             texture(emissiveMap, inUV).r * push.emissiveFactor);
        }
    }
}
