Shader "Clouds/Reconstruct"
{
    ComputeShader
    {
        #version 460
        #extension GL_EXT_samplerless_texture_functions : enable

        // Constants
        #define BAYER_MATRIX_SIZE 16
        const int kBayerMatrix16[16] = int[16](
            0, 8, 2, 10,
            12, 4, 14, 6,
            3, 11, 1, 9,
            15, 7, 13, 5
        );

        layout(binding = 0, r32f) readonly uniform image2D inCloudDepthTexture;
        layout(binding = 1, rgba16f) readonly uniform image2D inCloudRenderTexture;
        layout(binding = 2, rgba16f) readonly uniform image2D inCloudFogRenderTexture;
        layout(binding = 3, rgba16f) readonly uniform image2D inCloudReconstructionTextureHistory;
        layout(binding = 4, rgba16f) readonly uniform image2D inCloudFogReconstructionTextureHistory;
        layout(binding = 5, r32f) readonly uniform image2D inCloudDepthReconstructionTextureHistory;
        layout(binding = 6, rgba16f) writeonly uniform image2D imageCloudReconstructionTexture;
        layout(binding = 7, rgba16f) writeonly uniform image2D imageCloudFogReconstructionTexture;
        layout(binding = 8, r32f) writeonly uniform image2D imageCloudDepthReconstructionTexture;

        uniform sampler2D linearClampEdgeSampler;
        uniform sampler2D pointClampEdgeSampler;

        // Frame data structure
        struct FrameData
        {
            mat4 camViewProjPrev;
            mat4 camViewProjCur;
            mat4 camViewCur;
            mat4 camProjCur;
            vec4 frameIndex;
            uint bCameraCut;
            vec4 camPos;
            vec4 screenSize;
            vec4 time;
            // Add other fields as needed
        };

        // Frame data uniform buffer
        layout(std140, binding = 0) uniform FrameDataBlock
        {
            FrameData frameData;
        };

        // Utility function: Check if value is within range
        bool onRange(vec2 value, vec2 minVal, vec2 maxVal)
        {
            return all(greaterThanEqual(value, minVal)) && all(lessThanEqual(value, maxVal));
        }

        // Utility function: Get world position from UV and depth
        vec3 getWorldPos(vec2 uv, float depth, FrameData data)
        {
            // Convert UV to NDC (Normalized Device Coordinates)
            vec4 ndc = vec4(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
            
            // Inverse projection and view matrices
            mat4 invViewProj = inverse(data.camViewProjCur);
            vec4 worldPos = invViewProj * ndc;
            
            // Perspective divide
            return worldPos.xyz / worldPos.w;
        }

        // Variance clamping function for color reconstruction
        vec4 clampWithVariance(vec4 preColor, vec2 uv, vec2 texelSize, sampler2D sampler)
        {
            float wsum = 0.0;
            vec4 vsum = vec4(0.0);
            vec4 vsum2 = vec4(0.0);

            // 3x3 neighborhood
            for (int y = -1; y <= 1; ++y)
            {
                for (int x = -1; x <= 1; ++x)
                {
                    vec2 sampleUV = uv + texelSize * vec2(float(x), float(y));
                    vec4 neigh = texture(sampler, sampleUV);
                    float w = exp(-0.75 * (float(x) * float(x) + float(y) * float(y)));
                    vsum2 += neigh * neigh * w;
                    vsum += neigh * w;
                    wsum += w;
                }
            }

            vec4 ex = vsum / wsum;
            vec4 ex2 = vsum2 / wsum;
            vec4 dev = sqrt(max(ex2 - ex * ex, 0.0));

            const float boxSize = 2.5;
            vec4 nmin = ex - dev * boxSize;
            vec4 nmax = ex + dev * boxSize;

            return clamp(preColor, nmin, nmax);
        }

        // Main compute shader entry point
        layout (local_size_x = 8, local_size_y = 8) in;
        void main()
        {
            ivec2 texSize = imageSize(imageCloudReconstructionTexture);
            ivec2 workPos = ivec2(gl_GlobalInvocationID.xy);

            // Bounds check
            if(workPos.x >= texSize.x || workPos.y >= texSize.y)
            {
                return;
            }

            const vec2 uv = (vec2(workPos) + vec2(0.5)) / vec2(texSize);
            const vec2 curEvaluateCloudTexelSize = 1.0 / vec2(textureSize(inCloudRenderTexture, 0));
            
            // Get current cloud depth (quarter-resolution)
            ivec2 depthSamplePos = workPos / 4;
            const float traceCloudDepth = texelFetch(inCloudDepthTexture, depthSamplePos, 0).r;

            // Reproject to get previous UV
            vec3 worldPosCur = getWorldPos(uv, traceCloudDepth, frameData);
            vec4 projPosPrev = frameData.camViewProjPrev * vec4(worldPosCur, 1.0);
            vec3 projPosPrevH = projPosPrev.xyz / projPosPrev.w;

            vec2 uvPrev = projPosPrevH.xy * 0.5 + 0.5;
            uvPrev.y = 1.0 - uvPrev.y;

            bool bCameraCut = frameData.bCameraCut != 0;

            // Valid check for previous UV
            const bool bPrevUvValid = onRange(uvPrev, vec2(0.0), vec2(1.0)) && (!bCameraCut);

            vec4 color = vec4(0.0);
            vec4 fog = vec4(0.0);
            float depthZ = 0.0;

            if(bPrevUvValid)
            {
                // Fetch current data
                vec4 curColor = texelFetch(inCloudRenderTexture, depthSamplePos, 0);
                vec4 curFog = texelFetch(inCloudFogRenderTexture, depthSamplePos, 0);
                float curDepthZ = texelFetch(inCloudDepthTexture, depthSamplePos, 0).r;

                // Get previous depth from history
                float preDepthZ = texture(sampler2D(inCloudDepthReconstructionTextureHistory, linearClampEdgeSampler), uvPrev).r;

                // Bayer pattern for sparse evaluation
                uint bayerIndex = uint(frameData.frameIndex.x) % 16u;
                ivec2 bayerOffset = ivec2(kBayerMatrix16[bayerIndex] % 4, kBayerMatrix16[bayerIndex] / 4);
                ivec2 workDeltaPos = workPos % 4;
                const bool bUpdateEvaluate = (workDeltaPos.x == bayerOffset.x) && (workDeltaPos.y == bayerOffset.y);

                if(bUpdateEvaluate)
                {
                    depthZ = curDepthZ;
                    
                    // Update color with variance clamping
                    if(abs(preDepthZ - curDepthZ) > 0.1)
                    {
                        color = curColor;
                    }
                    else
                    {
                        vec4 preColor = texture(sampler2D(inCloudReconstructionTextureHistory, linearClampEdgeSampler), uvPrev);
                        
                        // Apply variance clamping to history color
                        vec4 clampColorHistory = clampWithVariance(preColor, uv, curEvaluateCloudTexelSize, sampler2D(inCloudRenderTexture, pointClampEdgeSampler));
                        
                        color = mix(clampColorHistory, curColor, 0.5);
                    }

                    // Update fog with variance clamping
                    if(abs(preDepthZ - curDepthZ) > 0.1)
                    {
                        fog = curFog;
                    }
                    else
                    {
                        vec4 preFog = texture(sampler2D(inCloudFogReconstructionTextureHistory, linearClampEdgeSampler), uvPrev);
                        
                        // Apply variance clamping to history fog
                        vec4 clampFogHistory = clampWithVariance(preFog, uv, curEvaluateCloudTexelSize, sampler2D(inCloudFogRenderTexture, pointClampEdgeSampler));
                        
                        fog = mix(clampFogHistory, curFog, 0.5);
                    }
                }
                else
                {
                    // No evaluation this frame, use history with reprojection
                    color = texture(sampler2D(inCloudReconstructionTextureHistory, linearClampEdgeSampler), uvPrev);
                    fog = texture(sampler2D(inCloudFogReconstructionTextureHistory, linearClampEdgeSampler), uvPrev);
                    depthZ = preDepthZ;
                }
            }
            else
            {
                // No history valid, bilinear sample from current frame
                color = texture(sampler2D(inCloudRenderTexture, linearClampEdgeSampler), uv);
                fog = texture(sampler2D(inCloudFogRenderTexture, linearClampEdgeSampler), uv);
                depthZ = texture(sampler2D(inCloudDepthTexture, linearClampEdgeSampler), uv).r;
            }

            // Write outputs
            imageStore(imageCloudReconstructionTexture, workPos, color);
            imageStore(imageCloudFogReconstructionTexture, workPos, fog);
            imageStore(imageCloudDepthReconstructionTexture, workPos, vec4(depthZ));
        }
    }
}