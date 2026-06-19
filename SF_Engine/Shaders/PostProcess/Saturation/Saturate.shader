Shader "SF/PostProcess/Saturate"{
    VertexShader{
        layout (location = 0) in vec3 aPos;
        void main() {
            gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0); 
        }
    }
    FragmentShader{
        layout(location = 0) in sampler2D sceneTexture;
        layout(location = 1) in float saturationAmount;
        layout(location = 2) in vec2 vUv;
        void main(){
            vec4 color = texture(sceneTexture, vUv);

            const vec3 luminanceWeights = vec3(0.2126, 0.7152, 0.0722);

            float luminance = dot(color.rgb, luminanceWeights);
            vec3 grayscale = vec3(luminance);
            
            vec3 finalColor = mix(grayscale, color.rgb, saturationAmount);
            
            gl_FragColor = vec4(finalColor, color.a);
        }
    }
}