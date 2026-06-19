Shader "SF/Atmosphere/Atmosphere"
{
    VertexShader
    {
        #version 450
        void main()
        {
            // Full-screen triangle from vertex index alone – no VBO needed.
            vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
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
            vec4  cameraPos;        // .xyz = world position relative to planet centre (metres)
            vec4  planetPos;        // unused – planet is at origin
            vec4  sunDir;           // .xyz = toward sun (unit), .w = sun intensity
            float bottomRadius;
            float topRadius;
            float renderUnitRadius; // unused
            float _p0;
            vec2  screenSize;
            vec2  _p1;
        } u;

        layout(set = 0, binding = 1) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 2) uniform sampler2D multiScatterLUT;
        layout(set = 0, binding = 3) uniform sampler2D skyViewLUT;

        #import "Atmosphere.si"

        vec3 sampleSkyView(vec3 rd)
        {
            float phi   = atan(rd.x, rd.z);                    // [-PI, PI]
            float theta = asin(clamp(rd.y, -1.0, 1.0));        // [-PI/2, PI/2]

            float u = (phi / (2.0 * PI)) + 0.5;                // remap to [0, 1]
            float v = skyViewEncodeV(theta);

            return textureLod(skyViewLUT, vec2(u, v), 0.0).rgb;
        }

        vec3 sunDisk(vec3 rd, vec3 sunDir, float sunIntensity,
                     float height, float bottomRadius, float topRadius)
        {
            const float SUN_ANGULAR_RADIUS = 0.0045;
            float cosAngle   = dot(rd, sunDir);
            float diskWeight = smoothstep(
                cos(SUN_ANGULAR_RADIUS * 1.05),
                cos(SUN_ANGULAR_RADIUS * 0.95),
                cosAngle);

            if (diskWeight <= 0.0) return vec3(0.0);

            // Attenuate by transmittance toward the sun so the disk dims at sunrise/set.
            float cosSun = dot(normalize(vec3(0.0, height, 0.0)), sunDir);
            vec3  T      = sampleTransmittance(transmittanceLUT, height, cosSun,
                                               bottomRadius, topRadius);
            return diskWeight * sunIntensity * T;
        }

        void main()
        {
            vec2 ndc = vec2(
                gl_FragCoord.x / u.screenSize.x * 2.0 - 1.0,
                gl_FragCoord.y / u.screenSize.y * 2.0 - 1.0);
            vec4 vp = u.invProj * vec4(ndc, 1.0, 1.0);
            vec3 rd  = normalize((u.invView * vec4(vp.xyz / vp.w, 0.0)).xyz);

            vec3  sunDir = normalize(u.sunDir.xyz);
            float sunI   = u.sunDir.w;
            float Rbot   = u.bottomRadius;
            float Rtop   = u.topRadius;

            vec3 viewPos = u.cameraPos.xyz;
            if (length(viewPos) < Rbot + 1.0)
                viewPos = normalize(viewPos.y >= 0.0 ? viewPos : vec3(0,1,0))
                          * (Rbot + 1.0);

            float camHeight = length(viewPos);

            vec2 atmoHit = raySphereIntersect(viewPos, rd, Rtop);
          

            float camAlt = camHeight - Rbot;
            if (camAlt < (Rtop - Rbot))
            {
                vec3 col = sampleSkyView(rd);

                col += sunDisk(rd, sunDir, sunI, camHeight, Rbot, Rtop);

                // Tone-map and write.
                col = 1.0 - exp(-col);

                // Alpha: derive transmittance along this pixel's ray to sky top.
                // Use the transmittance LUT for a cheap but physically consistent result.
                float cosSky   = dot(normalize(viewPos), rd);
                vec3  skyTrans = sampleTransmittance(transmittanceLUT, camHeight,
                                                     cosSky, Rbot, Rtop);
                float atmAlpha = 1.0 - dot(skyTrans, vec3(0.2126, 0.7152, 0.0722));

                outColor = vec4(col, atmAlpha);
                return;
            }


            float maxDist = atmoHit.y;
            vec2  gndHit  = raySphereIntersect(viewPos, rd, Rbot);
            if (gndHit.x > 0.0 && gndHit.x < gndHit.y)
                maxDist = gndHit.x;

            vec3 transmittance;
            vec3 col = calculateScattering(
                viewPos, rd, maxDist,
                sunDir, vec3(sunI),
                Rbot, Rtop,
                transmittanceLUT, multiScatterLUT,
                transmittance);

            col = 1.0 - exp(-col);

            float atmAlpha = 1.0 - dot(transmittance, vec3(0.2126, 0.7152, 0.0722));
            outColor = vec4(col, atmAlpha);
        }
    }
}
