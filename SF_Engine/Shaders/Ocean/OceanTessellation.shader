Shader "Ocean/OceanTessellation"
{
    Properties
    {
        @group("Wave")
        WaveAmplitude("Wave Amplitude", Float) = 1.2
        @group("Wave")
        WaveFrequency("Wave Frequency", Float) = 0.25
        @group("Wave")
        WaveSpeed("Wave Speed", Float) = 1.2
        @group("Wave")
        WaveSteepness("Wave Steepness", Range(0, 1)) = 0.45

        @group("Wave")
        WaveAmplitude2("Wave Amplitude 2", Float) = 0.5
        @group("Wave")
        WaveFrequency2("Wave Frequency 2", Float) = 0.6
        @group("Wave")
        WaveSpeed2("Wave Speed 2", Float) = 1.8

        @group("Visual")
        OceanColor("Ocean Color", Color) = (0.02, 0.05, 0.12, 1.0)
        @group("Visual")
        Glossiness("Glossiness", Range(0, 1)) = 0.92
        @group("Visual")
        ShallowColor("Shallow Color", Color) = (0.04, 0.22, 0.28, 1.0)
        @group("Visual")
        SpecularPower("Specular Power", Float) = 96.0
        @group("Visual")
        FoamColor("Foam Color", Color) = (0.85, 0.92, 0.95, 1.0)
        @group("Visual")
        FoamIntensity("Foam Intensity", Range(0, 1)) = 0.70
        @group("Visual")
        FoamThreshold("Foam Threshold", Range(0, 1)) = 0.18

        @group("Tessellation")
        TessFactor("Tess Factor", Range(1, 64)) = 32.0
        @group("Tessellation")
        MinTessDistance("Min Tess Distance", Float) = 20.0
        @group("Tessellation")
        MaxTessDistance("Max Tess Distance", Float) = 6000.0

        @group("Animation")
        TimeScale("Time Scale", Float) = 1.0
        @group("Animation")
        WindDirection("Wind Direction", Vector) = (1.0, 0.0, 0.3, 0.0)
    }

    ResourceLayout
    {
        [0, 0] uniformbuf FrameUBO
    }

    #pragma require tessellation_shaders
    #pragma specialize MAX_TESS_FACTOR = 32

    Pass "Ocean"
    {
        Cull Back
        ZWrite On
        Blend One Zero

        VertexShader
        {
            #pragma entry main
            #Section Includes
            #import "OceanCommon.si"

            layout(location = 0) in vec3 inPosition;
            layout(location = 1) in vec2 inUV;
            layout(location = 2) in vec3 inNormal;

            layout(location = 0) out vec3 outWorldPos;
            layout(location = 1) out vec2 outUV;
            layout(location = 2) out vec3 outNormal;

            void main()
            {
                outWorldPos = inPosition;
                outUV       = inUV;
                outNormal   = inNormal;
                gl_Position = vec4(inPosition, 1.0);
            }
        }

        TessellationControl
        {
            #pragma entry main
            #Section Includes
            #import "OceanCommon.si"

            layout(vertices = 4) out;

            layout(location = 0) in  vec3 inWorldPos[];
            layout(location = 1) in  vec2 inUV[];
            layout(location = 2) in  vec3 inNormal[];

            layout(location = 0) out vec3 outWorldPos[];
            layout(location = 1) out vec2 outUV[];
            layout(location = 2) out vec3 outNormal[];

            float computeTessLevel(vec3 worldPos)
            {
                float dist = distance(worldPos, frame.cameraPos);
                float t    = clamp(
                    (dist - frame.minTessDistance) /
                    max(frame.maxTessDistance - frame.minTessDistance, 0.001),
                    0.0, 1.0);
                return mix(frame.tessFactor, 1.0, t);
            }

            void main()
            {
                outWorldPos[gl_InvocationID] = inWorldPos[gl_InvocationID];
                outUV[gl_InvocationID]       = inUV[gl_InvocationID];
                outNormal[gl_InvocationID]   = inNormal[gl_InvocationID];

                if (gl_InvocationID == 0)
                {
                    float tl0 = computeTessLevel(inWorldPos[0]);
                    float tl1 = computeTessLevel(inWorldPos[1]);
                    float tl2 = computeTessLevel(inWorldPos[2]);
                    float tl3 = computeTessLevel(inWorldPos[3]);

                    gl_TessLevelOuter[0] = mix(tl3, tl0, 0.5);
                    gl_TessLevelOuter[1] = mix(tl0, tl1, 0.5);
                    gl_TessLevelOuter[2] = mix(tl1, tl2, 0.5);
                    gl_TessLevelOuter[3] = mix(tl2, tl3, 0.5);

                    gl_TessLevelInner[0] = max(gl_TessLevelOuter[1], gl_TessLevelOuter[3]);
                    gl_TessLevelInner[1] = max(gl_TessLevelOuter[0], gl_TessLevelOuter[2]);
                }
            }
        }

        TessellationEval
        {
            #pragma entry main
            #Section Includes
            #import "OceanCommon.si"

            layout(quads, fractional_odd_spacing, ccw) in;

            layout(set = 0, binding = 1) uniform sampler2DArray DisplacementTextures;

            layout(location = 0) in  vec3  inWorldPos[];
            layout(location = 1) in  vec2  inUV[];
            layout(location = 2) in  vec3  inNormal[];

            layout(location = 0) out vec3  outWorldPos;
            layout(location = 1) out vec2  outUV;
            layout(location = 2) out vec3  outNormal;
            layout(location = 3) out float outFoam;
            layout(location = 4) out float outWaveHeight;

            // Tiling scales per cascade, match OceanFFTSettings lengthScaleN.
            // World-space UV is derived from local tangent-plane coords so
            // tiling stays seamless under sphere projection.
            vec3 SampleDisplacement(vec3 localUV)
            {
                vec3 d0 = texture(DisplacementTextures, vec3(localUV.xy * frame.tile0, 0)).xyz;
                vec3 d1 = texture(DisplacementTextures, vec3(localUV.xy * frame.tile1, 1)).xyz;
                vec3 d2 = texture(DisplacementTextures, vec3(localUV.xy * frame.tile2, 2)).xyz;
                vec3 d3 = texture(DisplacementTextures, vec3(localUV.xy * frame.tile3, 3)).xyz;
                return d0 + d1 + d2 + d3;
            }

            void main()
            {
                vec3 posBot  = mix(inWorldPos[0], inWorldPos[1], gl_TessCoord.x);
                vec3 posTop  = mix(inWorldPos[3], inWorldPos[2], gl_TessCoord.x);
                vec3 worldPos = mix(posBot, posTop, gl_TessCoord.y);

                vec2 uvBot = mix(inUV[0], inUV[1], gl_TessCoord.x);
                vec2 uvTop = mix(inUV[3], inUV[2], gl_TessCoord.x);
                vec2 uv    = mix(uvBot, uvTop, gl_TessCoord.y);

                // Local radial frame at this point on the sphere (the base
                // mesh is already projected onto the sphere of radius
                // frame.planetRadius).
                vec3 up = normalize(worldPos - frame.planetCenter);
                vec3 tangentU, tangentV;
                BuildTangentFrame(up, tangentU, tangentV);

                // Camera-relative tangent-plane coords, used as the FFT
                // texture UV so phase stays stable as the camera moves
                // (mirrors the localOrigin trick from the Gerstner path,
                // raw worldPos has magnitude ~planetRadius and would
                // alias the texture lookup).
                vec3 localOrigin = worldPos - frame.cameraPos;
                vec2 localUV = vec2(dot(localOrigin, tangentU), dot(localOrigin, tangentV));

                vec3 displacement = SampleDisplacement(vec3(localUV, 0.0));

                // Apply displacement in the local tangent frame: x/z ->
                // horizontal (tangentU/tangentV), y -> radial (up).
                vec3 displaced = worldPos
                    + tangentU * displacement.x
                    + tangentV * displacement.z
                    + up * displacement.y;

                // Foam channel comes from cascade 0's alpha (accumulated
                // jacobian-based foam from CS_AssembleMaps).
                vec4 foamSample = texture(DisplacementTextures, vec3(localUV * frame.tile0, 0));

                outWorldPos   = displaced;
                outUV         = uv;
                outNormal     = up; // refined per-pixel from slope textures in fragment stage
                outFoam       = clamp(foamSample.a, 0.0, 1.0);
                outWaveHeight = displacement.y;

                gl_Position = frame.viewProj * vec4(displaced, 1.0);
            }
        }
        FragmentShader
        {
            #pragma entry main
            #Section Includes
            #import "OceanCommon.si"

            layout(set = 0, binding = 2) uniform sampler2DArray SlopeTextures;

            layout(location = 0) in  vec3  inWorldPos;
            layout(location = 1) in  vec2  inUV;
            layout(location = 2) in  vec3  inNormal;
            layout(location = 3) in  float inFoam;
            layout(location = 4) in  float inWaveHeight;

            layout(location = 0) out vec4  outColor;

            vec2 SampleSlopes(vec2 localUV)
            {
                vec2 s0 = texture(SlopeTextures, vec3(localUV * frame.tile0, 0)).xy;
                vec2 s1 = texture(SlopeTextures, vec3(localUV * frame.tile1, 1)).xy;
                vec2 s2 = texture(SlopeTextures, vec3(localUV * frame.tile2, 2)).xy;
                vec2 s3 = texture(SlopeTextures, vec3(localUV * frame.tile3, 3)).xy;
                return (s0 + s1 + s2 + s3) * frame.normalStrength;
            }

            void main()
            {
                vec3 up = normalize(inWorldPos - frame.planetCenter);
                vec3 tangentU, tangentV;
                BuildTangentFrame(up, tangentU, tangentV);

                vec3 localOrigin = inWorldPos - frame.cameraPos;
                vec2 localUV = vec2(dot(localOrigin, tangentU), dot(localOrigin, tangentV));

                vec2 slopes = SampleSlopes(localUV);

                // Slopes are in (x,z)-tangent space; combine with `up`.
                vec3 N = normalize(up - tangentU * slopes.x - tangentV * slopes.y);

                vec3 V = normalize(frame.cameraPos - inWorldPos);
                vec3 L = normalize(frame.sunDirection);
                vec3 H = normalize(L + V);
                vec3 R = reflect(-V, N);

                float NdotL = max(dot(N, L), 0.0);
                float NdotH = max(dot(N, H), 0.0);
                float NdotV = max(dot(N, V), 0.001);

                float fresnel = FresnelSchlick(NdotV);
                float roughness = clamp(1.0 - frame.glossiness, 0.025, 1.0);

                float D = GGX_D(NdotH, roughness);
                vec3 sunColor = vec3(1.05, 0.96, 0.82);
                vec3 specular = (sunColor * D * NdotL * 0.05) / (4.0 * NdotL * NdotV + 0.01);

                vec3 skyRefl = SkyColor(vec3(0.0, dot(R, up), 0.0));

                float VdotL = max(dot(V, L), 0.0);
                float crestFactor = clamp(inWaveHeight / max(frame.waveAmplitude, 0.001), 0.0, 1.0);
                float sss = pow(VdotL, 4.0) * crestFactor;
                vec3 sssColor = vec3(0.02, 0.50, 0.35) * sss * 1.8;

                float heightNorm = clamp((inWaveHeight + frame.waveAmplitude)
                                            / max(frame.waveAmplitude * 2.0, 0.001), 0.0, 1.0);
                float smoothProxy = smoothstep(0.0, 1.0, heightNorm);
                vec3 waterColor = mix(frame.oceanColor, frame.shallowColor, smoothProxy);

                vec3 refractColor = waterColor * (0.08 + 0.92 * NdotL) + sssColor;
                vec3 baseColor = mix(refractColor, skyRefl, fresnel);
                vec3 finalColor = baseColor + specular;

                float jacobianFoam = smoothstep(frame.foamThreshold, frame.foamThreshold + 0.25, inFoam);
                float steepFoam = smoothstep(0.72, 0.95, 1.0 - dot(N, up)) * 0.45;
                float foamMask = max(jacobianFoam, steepFoam) * frame.foamIntensity;

                finalColor = mix(finalColor, frame.foamColor, foamMask);

                outColor = vec4(finalColor, 1.0);
            }
        }
    }
}