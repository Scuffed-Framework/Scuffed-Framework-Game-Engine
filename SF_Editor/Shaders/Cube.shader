Shader "Editor/Cube"
{
    VertexShader
    {
        layout(location = 0) in vec3 inPosition;
        layout(location = 1) in vec3 inNormal;
        layout(location = 2) in vec2 inTexCoord;

        layout(binding = 0) uniform UBO {
            mat4 model;
            mat4 view;
            mat4 proj;
        } ubo;

        layout(location = 0) out vec3 outNormal;
        layout(location = 1) out vec3 outFragPos;

        void main() {
            vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
            outFragPos = worldPos.xyz;
            outNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
            gl_Position = ubo.proj * ubo.view * worldPos;
        }
    }

    FragmentShader
    {
        layout(location = 0) in vec3 inNormal;
        layout(location = 1) in vec3 inFragPos;

        layout(location = 0) out vec4 outColor;

        void main() {
            vec3 norm = normalize(inNormal);

            // Three-point lighting so all faces are visible
            vec3 key   = normalize(vec3( 1.0,  2.0,  2.0));
            vec3 fill  = normalize(vec3(-2.0,  1.0,  1.0));
            vec3 back  = normalize(vec3( 0.0, -1.0, -2.0));

            float dKey  = max(dot(norm, key),  0.0);
            float dFill = max(dot(norm, fill), 0.0) * 0.4;
            float dBack = max(dot(norm, back), 0.0) * 0.2;

            float light = 0.2 + dKey + dFill + dBack; // 0.2 ambient

            vec3 color = vec3(0.15, 0.45, 0.9);
            outColor = vec4(color * light, 1.0);
        }
    }
}
