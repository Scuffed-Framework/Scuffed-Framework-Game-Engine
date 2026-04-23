Shader "SF/Atmosphere/Atmosphere"
{
    VertexShader
    {
        #version 450
        void main()
        {
            vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(uv * 2.0 - 1.0, 0.9999, 1.0);
        }
    }

    FragmentShader
    {
        #version 450
        layout(location = 0) out vec4 outColor;

        layout(set = 0, binding = 0) uniform AtmoUBO
        {
            mat4  invProj;
            mat4  invView;
            vec4  cameraPos;        // .xyz = viewPos in metres (cam - planet centre)
            vec4  planetPos;        // unused (planet at origin)
            vec4  sunDir;           // .xyz = toward sun (unit), .w = sunIntensity
            float bottomRadius;
            float topRadius;
            float renderUnitRadius; // unused
            float _p0;
            vec2  screenSize;
            vec2  _p1;
        } u;

        layout(set = 0, binding = 1) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 2) uniform sampler2D multiScatterLUT;

        //  Constants  must match both LUT shaders exactly 
        const vec3  RAY_BETA          = vec3(5.5e-6, 13.0e-6, 22.4e-6);
        const vec3  MIE_BETA          = vec3(21e-6);
        const vec3  AMBIENT_BETA      = vec3(0.0);
        const vec3  ABSORPTION_BETA   = vec3(2.04e-5, 4.97e-5, 1.95e-6);
        const float G                 = 0.7;
        const float HEIGHT_RAY        = 8e3;
        const float HEIGHT_MIE        = 1.2e3;
        const float HEIGHT_ABSORPTION = 30e3;
        const float ABSORPTION_FALLOFF= 4e3;
        const int   PRIMARY_STEPS     = 32;

        //  LUT helpers 
        // UV convention shared by both LUTs:
        //   x = (height - bottomRadius) / (topRadius - bottomRadius)
        //   y = cosSun * 0.5 + 0.5
        vec2 atmosUV(float height, float cosSun)
        {
            float x = clamp((height - u.bottomRadius) / (u.topRadius - u.bottomRadius), 0.0, 1.0);
            float y = clamp(cosSun * 0.5 + 0.5, 0.0, 1.0);
            return vec2(x, y);
        }

        vec3 sampleTransmittance(float height, float cosSun)
        {
            return textureLod(transmittanceLUT, atmosUV(height, cosSun), 0.0).rgb;
        }

        vec3 sampleMultiScatter(float height, float cosSun)
        {
            return textureLod(multiScatterLUT, atmosUV(height, cosSun), 0.0).rgb;
        }

        //  Ray-sphere intersect 
        vec2 raySphereIntersect(vec3 start, vec3 dir, float radius)
        {
            float a = dot(dir, dir);
            float b = 2.0 * dot(dir, start);
            float c = dot(start, start) - radius * radius;
            float d = b*b - 4.0*a*c;
            if (d < 0.0) return vec2(1e5, -1e5);
            return vec2((-b - sqrt(d)) / (2.0*a),
                        (-b + sqrt(d)) / (2.0*a));
        }

        //  Scattering integral 
        vec3 calculateScattering(
            vec3 start, vec3 dir, float maxDist,
            vec3 lightDir, vec3 lightIntensity,
            float planetRadius, float atmosRadius)
        {
            float a = dot(dir, dir);
            float b = 2.0 * dot(dir, start);
            float c = dot(start, start) - atmosRadius * atmosRadius;
            float d = b*b - 4.0*a*c;
            if (d < 0.0) return vec3(0.0);

            vec2 rayLength = vec2(
                max((-b - sqrt(d)) / (2.0*a), 0.0),
                min((-b + sqrt(d)) / (2.0*a), maxDist));
            if (rayLength.x > rayLength.y) return vec3(0.0);

            float stepSize = (rayLength.y - rayLength.x) / float(PRIMARY_STEPS);
            float rayPos   = rayLength.x + stepSize * 0.5;

            vec3 totalRay = vec3(0.0);
            vec3 totalMie = vec3(0.0);
            vec3 totalMS  = vec3(0.0);
            vec3 optI     = vec3(0.0);

            float mu     = dot(dir, lightDir);
            float mumu   = mu * mu;
            float gg     = G * G;
            float phaseR = 3.0 / (16.0 * 3.14159265) * (1.0 + mumu);
            float phaseM = 3.0 / (8.0  * 3.14159265) * ((1.0 - gg) * (mumu + 1.0))
                           / (pow(1.0 + gg - 2.0*mu*G, 1.5) * (2.0 + gg));

            for (int i = 0; i < PRIMARY_STEPS; i++)
            {
                vec3  posI     = start + dir * rayPos;
                float heightI  = length(posI) - planetRadius;

                // Density at this sample
                vec3  density  = vec3(exp(-heightI / HEIGHT_RAY),
                                      exp(-heightI / HEIGHT_MIE),
                                      0.0);
                float denom    = (HEIGHT_ABSORPTION - heightI) / ABSORPTION_FALLOFF;
                density.z      = (1.0 / (denom*denom + 1.0)) * density.x;
                density       *= stepSize;
                optI           += density;

                // Transmittance from camera to this sample
                vec3 camTrans = exp(
                    - RAY_BETA        * optI.x
                    - MIE_BETA        * optI.y
                    - ABSORPTION_BETA * optI.z);

                // Sun transmittance via LUT
                float cosSunI  = dot(normalize(posI), lightDir);
                float hI       = length(posI);
                vec3  sunTrans = sampleTransmittance(hI, cosSunI);

                vec3 attn = camTrans * sunTrans;

                totalRay += density.x * attn;
                totalMie += density.y * attn;

                // Multiple scattering  isotropic, no phase needed (baked into LUT)
                vec3 ms      = sampleMultiScatter(hI, cosSunI);
                vec3 sigma_s = RAY_BETA * (density.x / stepSize)
                             + MIE_BETA * (density.y / stepSize);
                totalMS += camTrans * sigma_s * ms * stepSize;

                rayPos += stepSize;
            }

            vec3 opacity = exp(-(MIE_BETA        * optI.y
                               + RAY_BETA        * optI.x
                               + ABSORPTION_BETA * optI.z));

            return (phaseR * RAY_BETA * totalRay
                  + phaseM * MIE_BETA * totalMie
                  + totalMS
                  + optI.x * AMBIENT_BETA) * lightIntensity;
        }

        //  Main 
        void main()
        {
            vec2 ndc = vec2(
                gl_FragCoord.x / u.screenSize.x * 2.0 - 1.0,
                gl_FragCoord.y / u.screenSize.y * 2.0 - 1.0);
            vec4 vp = u.invProj * vec4(ndc, 1.0, 1.0);
            vec3 rd = normalize((u.invView * vec4(vp.xyz / vp.w, 0.0)).xyz);

            vec3  sunDir = normalize(u.sunDir.xyz);
            float sunI   = u.sunDir.w;
            float Rbot   = u.bottomRadius;
            float Rtop   = u.topRadius;

            vec3 viewPos = u.cameraPos.xyz;
            if (length(viewPos) < Rbot + 1.0)
                viewPos = normalize(viewPos.y >= 0.0 ? viewPos : vec3(0,1,0)) * (Rbot + 1.0);

            vec2 atmoHit = raySphereIntersect(viewPos, rd, Rtop);
            if (atmoHit.x > atmoHit.y)
            {
                // Ray missed the atmosphere entirely.
                // From ground level: show ground haze so the horizon isn't black.
                // From space: output transparent so the skybox/space shows through.
                float camAlt = length(viewPos) - Rbot;
                if (camAlt < 1000.0)
                    outColor = vec4(0.72, 0.82, 0.90, 1.0); // ground haze
                else
                    outColor = vec4(0.0, 0.0, 0.0, 0.0);    // transparent space
                return;
            }

            float maxDist = atmoHit.y;
            vec2  gndHit  = raySphereIntersect(viewPos, rd, Rbot);
            if (gndHit.x > 0.0 && gndHit.x < gndHit.y)
                maxDist = gndHit.x;

            vec3 col = calculateScattering(
                viewPos, rd, maxDist,
                sunDir, vec3(sunI),
                Rbot, Rtop);

            col = 1.0 - exp(-col);
            outColor = vec4(col, 1.0);
        }
    }
}