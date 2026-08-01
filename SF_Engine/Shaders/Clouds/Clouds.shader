Shader "SF/Clouds/Clouds"
{
    VertexShader
    {
        #version 450
        void main()
        {
            // Fullscreen triangle  (same pattern as Atmosphere.shader)
            vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
        }
    }

    FragmentShader
    {
        #version 450
        #import "Clouds/CloudScatterFunctions.si"
        layout(location = 0) out vec4 outColor; // premultiplied alpha

        layout(set = 0, binding = 0) uniform AtmoUBO
        {
            mat4  invProj;
            mat4  invView;
            vec4  cameraPos;         // .xyz = planet-relative world metres
            vec4  planetPos;         // always vec4(0)
            vec4  sunDir;            // .xyz = toward sun (unit),  .w = intensity
            float bottomRadius;
            float topRadius;
            float renderUnitRadius;
            float _p0;
            vec2  screenSize;
            vec2  _p1;
        } atmo;

        layout(set = 0, binding = 1) uniform CloudUBO
        {
            float cloudBottomRadius; // params.bottomRadius + minAlt
            float cloudTopRadius;    // params.bottomRadius + maxAlt
            float stepCount;         // primary ray step budget  (float for shader compat)
            float lightStepCount;    // kept for ABI; unused in shader (precomputed accum)

            float cloudDensityScale; // global density multiplier
            float cloudCoverage;     // [0, 1] coverage bias

            float time; // accumulated seconds, used for wind offset

            float sdfRangeMetres; // max SDF distance in G channel
            int frameIndex; // frame counter for blue noise sampling
            int bozo[3]; // padding
        };

        layout(set = 0, binding = 2) uniform sampler2D blueNoise;       // 128x128 2D
        layout(set = 0, binding = 3) uniform sampler3D worleyNoise;     // 128^3 RGBA8
        layout(set = 0, binding = 4) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 5) uniform sampler2D multiScatterLUT;
        layout(set = 0, binding = 6) uniform sampler3D baseNoise;         // 128^3 R8
        layout(set = 0, binding = 7) uniform sampler3D detailNoise;       // 128^3 R8
        layout(set = 0, binding = 8) uniform sampler2D aerialPerspRange; // distToTravel per XY

        float depthToViewDist(float depth, vec2 ndc)
        {
            vec4 clipPos = vec4(ndc, depth, 1.0);
            vec4 vPos    = atmo.invProj * clipPos;
            return length(vPos.xyz / vPos.w);
        }


        const int MAX_STEPS = 32;
        const int LIGHT_STEPS = 8;

        // ---- ray/sphere, returns (near, far) intersection distances ----
        vec2 RaySphere(vec3 center, float radius, vec3 ro, vec3 rd)
        {
            vec3 oc = ro - center;
            float b = dot(oc, rd);
            float c = dot(oc, oc) - radius * radius;
            float disc = b * b - c;
            if (disc < 0.0)
                return vec2(1e9, -1.0);
            float s = sqrt(disc);
            return vec2(-b - s, -b + s);
        }

        struct ShellHit
        {
            float dstToShell;
            float dstThroughShell;
        };

        // Intersects the ray with the spherical shell between innerR and outerR,
        // handling camera below / inside / above the shell.
        ShellHit IntersectCloudShell(vec3 ro, vec3 rd, float innerR, float outerR, float groundR)
        {
            ShellHit hit;
            hit.dstToShell = 0.0;
            hit.dstThroughShell = 0.0;

            vec2 outerHit = RaySphere(vec3(0.0), outerR, ro, rd);
            if (outerHit.y < 0.0)
                return hit;

            vec2 innerHit  = RaySphere(vec3(0.0), innerR,  ro, rd);
            vec2 groundHit = RaySphere(vec3(0.0), groundR, ro, rd);

            // If solid ground is ahead of the camera along this ray, nothing beyond
            // it (including any "far side" cloud shell) can ever be visible.
            float maxDst = (groundHit.y >= 0.0 && groundHit.x > 0.0) ? groundHit.x : 1e9;

            float h = length(ro);
            float startDst, endDst;

            if (h > outerR)
            {
                if (outerHit.x < 0.0)
                    return hit;
                startDst = outerHit.x;
                endDst   = outerHit.y;
                if (innerHit.y >= 0.0 && innerHit.x > 0.0)
                    endDst = innerHit.x;
            }
            else if (h < innerR)
            {
                if (innerHit.y < 0.0)
                    return hit;
                startDst = max(innerHit.y, 0.0);
                endDst   = outerHit.y;
            }
            else
            {
                startDst = 0.0;
                endDst   = outerHit.y;
                if (innerHit.y >= 0.0 && innerHit.x > 0.0)
                    endDst = innerHit.x;
            }

            // Clip both ends against the ground occluder.
            startDst = min(startDst, maxDst);
            endDst   = min(endDst, maxDst);

            hit.dstToShell = startDst;
            hit.dstThroughShell = max(0.0, endDst - startDst);
            return hit;
        }

        float GetCloudDensity(vec3 posMetres)
        {
            float height = length(posMetres);
            float heightFrac = clamp((height - cloudBottomRadius) /
                                      max(cloudTopRadius - cloudBottomRadius, 1.0),
                                      0.0, 1.0);

            // Soft falloff at base and top of the layer
            float heightGradient = smoothstep(0.0, 0.2, heightFrac) *
                                    (1.0 - smoothstep(0.6, 1.0, heightFrac));
            if (heightGradient <= 0.0)
                return 0.0;

            vec3 wind = vec3(time * 20.0, time * 5.0, time * 12.0);
            vec3 samplePos = (posMetres + wind) * 0.000008; // tile scale, tune per-project

            float baseShape = texture(worleyNoise, samplePos).r;

            float cov = clamp(cloudCoverage, 0.0, 1.0);
            float shaped = max(baseShape - (1.0 - cov), 0.0);

            return shaped * heightGradient * cloudDensityScale;
        }

        float CloudSunMarch(vec3 pos, vec3 sunDir)
        {
            float stepSize = (cloudTopRadius - cloudBottomRadius) / float(LIGHT_STEPS) * 0.5;
            float densitySum = 0.0;
            vec3 p = pos;

            for (int i = 0; i < LIGHT_STEPS; i++)
            {
                p += sunDir * stepSize;
                densitySum += GetCloudDensity(p) * stepSize;
            }
            return exp(-densitySum);
        }

        vec4 CloudMarch(vec3 origin, vec3 dir, vec3 sunDir, float dstToShell, float dstThroughShell)
        {
            int steps = min(int(stepCount), MAX_STEPS);
            if (steps <= 0 || dstThroughShell <= 0.0)
                return vec4(0.0);

            float stepSize = dstThroughShell / float(steps);

            // Dither the starting offset with blue noise to hide banding
            ivec2 jitter = ivec2(
                (frameIndex * 73) % 128, 
                (frameIndex * 31) % 128
            );

            ivec2 pix = (ivec2(gl_FragCoord.xy) + jitter) % 128;
            float noiseOffset = texelFetch(blueNoise, pix, 0).r;
            float dst = dstToShell + noiseOffset * stepSize;

            float transmittance = 1.0;
            vec3 luminance = vec3(0.0);

            float sunIntensity = atmo.sunDir.w;
            vec3 sunColor = vec3(1.0, 0.98, 0.92) * sunIntensity;
            vec3 ambientColor = vec3(0.4, 0.5, 0.6) * 0.3;

            // Calculate phase for sunlight scattering
            float cosTheta = dot(dir, sunDir);
            // g = 0.6 is a good starting point for forward scattering in clouds
            // replace with GetPhaseFunctions 
            float phaseVal = HenyeyGreensteinPhaseFunction(cosTheta, 0.6); 

            for (int i = 0; i < steps; i++)
            {
                vec3 pos = origin + dir * dst;
                float density = GetCloudDensity(pos);

                if (density > 0.0)
                {
                    float sunTransmittance = CloudSunMarch(pos, sunDir);
                    
                    // 1. Apply Phase Function to direct sunlight
                    vec3 directLight = sunColor * sunTransmittance * phaseVal;
                    
                    // 2. Attenuate ambient light! 
                    // The deeper we are (lower transmittance), the darker the ambient light should be.
                    vec3 ambientLight = ambientColor * (0.2 + 0.8 * sunTransmittance); 

                    // Combine and multiply by density and step size
                    vec3 lightEnergy = (directLight + ambientLight) * density * stepSize;

                    luminance += lightEnergy * transmittance;
                    transmittance *= exp(-density * stepSize);

                    if (transmittance < 0.01)
                        break;
                }

                dst += stepSize;
            }

            return vec4(luminance, 1.0 - transmittance);
        }

        void main()
        {
            vec2 uv = gl_FragCoord.xy / atmo.screenSize;
            vec2 ndc = uv * 2.0 - 1.0;

            vec4 clip = vec4(ndc, 1.0, 1.0);
            vec4 viewPos = atmo.invProj * clip;
            viewPos /= viewPos.w;
            vec3 rayDir = normalize((atmo.invView * vec4(viewPos.xyz, 0.0)).xyz);
            
            vec3 rayOrigin = atmo.cameraPos.xyz;
            vec3 sunDir = atmo.sunDir.xyz;

            float depth = texture(sceneDepth, uv).r;
            float sceneDist = (depth > 0.0) ? depthToViewDist(depth, ndc) : 1e9;

            ShellHit hit = IntersectCloudShell(rayOrigin, rayDir, cloudBottomRadius, cloudTopRadius, atmo.bottomRadius);

            hit.dstThroughShell = max(0.0, min(hit.dstToShell + hit.dstThroughShell, sceneDist) - hit.dstToShell);

            if (hit.dstThroughShell <= 0.0)
            {
                outColor = vec4(0.0);
                return;
            }

            outColor = CloudMarch(rayOrigin, rayDir, sunDir, hit.dstToShell, hit.dstThroughShell);

            // 1. Exposure Control (Tweak this value! Try 0.1, 0.01, etc.)
            float exposure = 1.0; 
            vec3 exposedColor = outColor.rgb * exposure;

            // 2. Simple Reinhard Tonemapping (compresses high values down gracefully)
            outColor.rgb = exposedColor / (1.0 + exposedColor);
        }
    }
}