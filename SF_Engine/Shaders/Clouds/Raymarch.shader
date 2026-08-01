Shader "Clouds/Raymarch"
{
    ComputeShader
    {
        #version 460
        #extension GL_EXT_samplerless_texture_functions : enable

        #import "Clouds/CloudScatterFunctions.si"
        // Constants
        #define BAYER_MATRIX_SIZE 16
        const int kBayerMatrix16[16] = int[16](
            0, 8, 2, 10,
            12, 4, 14, 6,
            3, 11, 1, 9,
            15, 7, 13, 5
        );

        // Output textures
        layout(binding = 0, rgba16f) writeonly uniform image2D imageCloudRenderTexture;
        layout(binding = 1, r32f) writeonly uniform image2D imageCloudDepthTexture;
        layout(binding = 2, rgba16f) writeonly uniform image2D imageCloudFogRenderTexture;

        layout(set = 0, binding = 3) uniform AtmoUBO
        {
            mat4  invProj;
            mat4  invView;
            vec4  cameraPos;
            vec4  planetPos;
            vec4  sunDir;
            float bottomRadius;
            float topRadius;
            float renderUnitRadius;
            float _p0;
            vec2  screenSize;
            vec2  _p1;
        } u;


        // Noise functions
        float rand3DPCG16(ivec3 p)
        {
            ivec3 q = p * ivec3(1664525, 1664525, 1664525) + ivec3(1013904223, 1013904223, 1013904223);
            int x = q.x ^ (q.y * 65536) ^ (q.z * 65536);
            return float(x & 0xFFFF) / 65536.0;
        }

        float interleavedGradientNoise(vec2 position, float frameIndex)
        {
            vec2 n = position + vec2(0.5, 0.5);
            vec2 f = fract(n);
            
            // Optimized interleaved gradient noise
            float a = dot(f, vec2(0.06711056, 0.00583715));
            float b = dot(floor(n), vec2(0.06711056, 0.00583715));
            return fract(a + b + frameIndex * 0.001);
        }

        float bayer64(vec2 p)
        {
            uint x = uint(p.x) & 7u;
            uint y = uint(p.y) & 7u;
            
            // Bayer matrix 8x8
            const uint bayer8x8[64] = uint[64](
                0, 32, 8, 40, 2, 34, 10, 42,
                48, 16, 56, 24, 50, 18, 58, 26,
                12, 44, 4, 36, 14, 46, 6, 38,
                60, 28, 52, 20, 62, 30, 54, 22,
                3, 35, 11, 43, 1, 33, 9, 41,
                51, 19, 59, 27, 49, 17, 57, 25,
                15, 47, 7, 39, 13, 45, 5, 37,
                63, 31, 55, 23, 61, 29, 53, 21
            );
            
            return float(bayer8x8[y * 8u + x]) / 64.0;
        }
        
        float BlueNoiseErrorDistrib(uint pixel_i, uint pixel_j, uint sampleIndex, uint sampleDimension)
        {
            // wrap arguments
            pixel_i = pixel_i & 127u;
            pixel_j = pixel_j & 127u;
            sampleIndex = sampleIndex & 255u;
            sampleDimension = sampleDimension & 255u;

            // xor index based on optimized ranking
            uint rankedSampleIndex = sampleIndex ^ rankingTile[sampleDimension + (pixel_i + pixel_j * 128u) * 8u];

            // fetch value in sequence
            uint value = sobol_256spp_256d[sampleDimension + rankedSampleIndex * 256u];

            // If the dimension is optimized, xor sequence value based on optimized scrambling
            value = value ^ scramblingTile[(sampleDimension%8) + (pixel_i + pixel_j * 128u) * 8u];

            // convert to float and return
            float v = (0.5f + value) / 256.0f;

            return v;
        }

        // Cloud ray marching and color computation
        vec4 cloudColorCompute(
            vec2 uv,
            float noiseSeed,
            out float depth,
            ivec2 workPos,
            vec3 worldDir,
            bool godRays,
            out vec4 fogLighting,
            float blueNoise
        )
        {
            // Initialize outputs
            depth = 1.0; // Reverse Z (far plane)
            fogLighting = vec4(0.0);
            
            // Cloud parameters
            float cloudAltitude = 1500.0f; // meters
            float cloudThickness = 4000.0f; // meters
            float cloudCoverage = 0.5f; // Replace with actual frameData value if available
            float cloudDensity = 0.1f; // Replace with actual frameData value if available
            
            // Ray origin from camera position
            vec3 rayOrigin = frameData.camPos.xyz;
            vec3 rayDir = worldDir;
            
            // Calculate intersection with cloud layer
            float cloudHeightStart = cloudAltitude;
            float cloudHeightEnd = cloudAltitude + cloudThickness;
            
            // Distance to cloud layer
            float tMin = 0.0;
            float tMax = 100000.0;
            
            // Find intersection with cloud layer (simplified)
            vec3 planetCenter = u.planetPos.xyz;
            float planetRadius = u.bottomRadius;
            
            // Check for cloud layer intersection
            float cloudDist = (cloudHeightStart - rayOrigin.y) / rayDir.y;
            float cloudDistEnd = (cloudHeightEnd - rayOrigin.y) / rayDir.y;
            
            if (cloudDist < 0.0 || cloudDistEnd < 0.0)
            {
                return vec4(0.0);
            }
            
            tMin = min(cloudDist, cloudDistEnd);
            tMax = max(cloudDist, cloudDistEnd);
            
            if (tMin < 0.0) tMin = 0.0;
            
            // Ray march through cloud
            float stepSize = (tMax - tMin) / 64.0; // 64 steps
            float totalDensity = 0.0;
            vec3 accumulatedLight = vec3(0.0);
            vec3 accumulatedFog = vec3(0.0);
            
            // Sample cloud density using noise
            for (int i = 0; i < 64; i++)
            {
                float t = tMin + float(i) * stepSize + blueNoise * stepSize;
                vec3 samplePos = rayOrigin + rayDir * t;
                
                // Calculate cloud density at this position
                float height = length(samplePos - planetCenter) - planetRadius;
                float localHeight = height - cloudAltitude;
                
                if (localHeight < 0.0 || localHeight > cloudThickness)
                {
                    continue;
                }
                
                // Noise-based density (simplified - in practice use 3D noise)
                float density = 1.0 - abs(localHeight / cloudThickness - 0.5) * 2.0;
                density *= cloudDensity * 0.1;
                
                // Coverage mask (simplified)
                float coverage = cloudCoverage;
                density *= coverage;
                
                // Calculate transmittance
                float transmittance = exp(-density * stepSize * 0.5);
                
                // Light scattering (simplified)
                vec3 lightDir = frameData.sky.sunDirection;
                float cosAngle = dot(rayDir, lightDir);
                
                // Phase function (Henyey-Greenstein)
                float g = HenyeyGreensteinPhaseFunction(cosAngle, 0.6);
                float phase = (1.0 - g * g) / (4.0 * 3.14159 * pow(1.0 + g * g - 2.0 * g * cosAngle, 1.5));
                
                // Sun light contribution
                vec3 sunColor = frameData.sky.sunColor * 2.0;
                vec3 lightContrib = sunColor * phase * density * stepSize;
                
                // Accumulate
                accumulatedLight += lightContrib * exp(-totalDensity);
                accumulatedFog += vec3(0.1) * density * stepSize;
                totalDensity += density * stepSize;
                
                // Early exit if fully opaque
                if (totalDensity > 3.0)
                {
                    break;
                }
            }
            
            // Calculate depth (reverse Z)
            float distance = tMin + totalDensity * 0.5;
            vec4 clipPos = frameData.camViewProj * vec4(rayOrigin + rayDir * distance, 1.0);
            depth = clipPos.z / clipPos.w;
            
            // Convert from linear depth to reverse Z (0=far, 1=near)
            depth = 1.0 - depth;
            
            // Output color with alpha
            vec3 cloudColor = accumulatedLight * 0.5;
            float alpha = 1.0 - exp(-totalDensity * 0.3);
            
            fogLighting = vec4(accumulatedFog, alpha);
            
            return vec4(cloudColor, alpha);
        }

        // Main compute shader
        layout (local_size_x = 8, local_size_y = 8) in;
        void main()
        {
            ivec2 texSize = imageSize(imageCloudRenderTexture);
            ivec2 workPos = ivec2(gl_GlobalInvocationID.xy);

            if(workPos.x >= texSize.x || workPos.y >= texSize.y)
            {
                return;
            }

            // Get bayer offset matrix
            uint bayerIndex = uint(frameData.frameIndex.x) % 16u;
            ivec2 bayerOffset = ivec2(kBayerMatrix16[bayerIndex] % 4, kBayerMatrix16[bayerIndex] / 4);

            // Get evaluate position in full resolution
            ivec2 fullResSize = texSize * 4;
            ivec2 fullResWorkPos = workPos * 4 + bayerOffset;

            // Get evaluate uv in full resolution
            const vec2 uv = (vec2(fullResWorkPos) + vec2(0.5)) / vec2(fullResSize);

            // Use blue noise texture (placeholder)
            float blueNoise = BlueNoiseErrorDistrib(
                uint(workPos.x), 
                uint(workPos.y), 
                0, 
                0u
            );
    
            // Offset retarget for new seeds each frame
            uvec2 offset = uvec2(vec2(0.754877669, 0.569840296) * frameData.frameIndex.x) * uvec2(texSize);
            uvec2 offsetId = uvec2(workPos) + offset;
            offsetId.x = offsetId.x % uint(texSize.x);
            offsetId.y = offsetId.y % uint(texSize.y);
            
            float blueNoise2 = BlueNoiseErrorDistrib(
                offsetId.x, 
                offsetId.y, 
                0, 
                0u
            );

            // Calculate world space direction
            vec4 clipSpace = vec4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
            vec4 viewPosH = frameData.camInvertProj * clipSpace;
            vec3 viewSpaceDir = viewPosH.xyz / viewPosH.w;
            vec3 worldDir = normalize((frameData.camInvertView * vec4(viewSpaceDir, 0.0)).xyz);

            float depth = 0.0; // reverse z
            vec4 fogLighting = vec4(0.0);

            // Compute cloud color
            vec4 cloudColor = cloudColorCompute( 
                uv, 
                blueNoise2, 
                depth, 
                workPos, 
                worldDir, 
                frameData.sky.cloudGodRay != 0, 
                fogLighting, 
                blueNoise2
            );

            // Store outputs
            imageStore(imageCloudRenderTexture, workPos, cloudColor);
            imageStore(imageCloudDepthTexture, workPos, vec4(depth));
            imageStore(imageCloudFogRenderTexture, workPos, fogLighting);
        }
    }
}