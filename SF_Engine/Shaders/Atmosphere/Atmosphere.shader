Shader "SF/Atmosphere/Atmosphere"
{
    Cull Off
    VertexShader
    {
        #version 450
        void main()
        {
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

        layout(set = 0, binding = 1) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 2) uniform sampler2D multiScatterLUT;
        layout(set = 0, binding = 3) uniform sampler2D skyViewLUT;

        layout(set = 0, binding = 4) uniform sampler3D aerialPerspColorRGBTransR; // inscatter.rgb, T.r
        layout(set = 0, binding = 5) uniform sampler3D aerialPerspTransGB;        // T.gb
        layout(set = 0, binding = 6) uniform sampler2D aerialPerspRange;          // distToTravel per XY

        layout(set = 0, binding = 7) uniform sampler2D sceneColor;  // HDR geometry colour
        layout(set = 0, binding = 8) uniform sampler2D sceneDepth;  // depth buffer [0,1]

        #import "Atmosphere.si"

        vec3 arbitraryPerp(vec3 n) 
        { 
            vec3 a = (abs(n.x) < 0.9) ? vec3(1, 0, 0) : vec3(0, 1, 0); 
            return normalize(cross(n, a)); 
        }

        vec3 sampleSkyView(vec3 rd, vec3 viewPos, vec3 sunDir, float camR, float botRadius)
        {
            vec3 up = normalize(viewPos);

            vec3  sunHoriz    = sunDir - dot(sunDir, up) * up;
            float sunHorizLen = length(sunHoriz);
            vec3  sunProj     = (sunHorizLen > 1e-4)
                                    ? (sunHoriz / sunHorizLen)
                                    : arbitraryPerp(up);
            vec3 perpAxis = cross(up, sunProj);

            float sinTh = clamp(dot(rd, up), -1.0, 1.0);
            float theta = asin(sinTh);
            float v     = skyViewEncodeV(theta, camR, botRadius);

            vec3  rdH    = rd - sinTh * up;
            float rdHLen = length(rdH);
            float u_coord;
            if (rdHLen < 1e-4)
            {
                u_coord = 0.0;
            }
            else
            {
                vec3  rdHoriz = rdH / rdHLen;
                float phi     = atan(dot(rdHoriz, perpAxis), dot(rdHoriz, sunProj));
                u_coord = abs(phi) / PI;
            }

            return textureLod(skyViewLUT, vec2(u_coord, v), 0.0).rgb;
        }

        vec3 sunDisk(vec3 rd, vec3 sunDir, float sunIntensity,
                     vec3 viewPos, float bottomRadius, float topRadius)
        {
            const float SUN_ANGULAR_RADIUS = 0.0045;
            float cosAngle   = dot(rd, sunDir);
            float diskWeight = smoothstep(
                cos(SUN_ANGULAR_RADIUS * 1.05),
                cos(SUN_ANGULAR_RADIUS * 0.95),
                cosAngle);
            if (diskWeight <= 0.0) return vec3(0.0);
            float camR   = length(viewPos);
            float cosSun = dot(viewPos / camR, sunDir);
            vec3  T      = sampleTransmittance(transmittanceLUT, camR, cosSun,
                                               bottomRadius, topRadius);
            return diskWeight * sunIntensity * T;
        }

        void sampleAerialPerspective(vec2 screenUV, float sceneDist,
                                     out vec3 outScatter, out vec3 outTransmit)
        {
            float maxDist = max(texture(aerialPerspRange, screenUV).r, 0.001);
            float t = clamp(sqrt(sceneDist / maxDist), 0.0, 1.0);  

            vec4 ct  = texture(aerialPerspColorRGBTransR, vec3(screenUV, t));
            vec2 tgb = texture(aerialPerspTransGB,        vec3(screenUV, t)).rg;

            outScatter  = ct.rgb;
            outTransmit = vec3(ct.a, tgb.r, tgb.g);
        }

        float depthToViewDist(float depth, vec2 ndc)
        {
            vec4 clipPos = vec4(ndc, depth, 1.0);
            vec4 vPos    = u.invProj * clipPos;
            return length(vPos.xyz / vPos.w);  
        }

        void main()
        {
            vec2 screenUV = gl_FragCoord.xy / u.screenSize;
            vec2 ndc = screenUV * 2.0 - 1.0;

            // Reconstruct world-space ray direction
            vec4 vp = u.invProj * vec4(ndc, 1.0, 1.0);
            vec3 rd  = normalize((u.invView * vec4(vp.xyz / vp.w, 0.0)).xyz);

            vec3  sunDir = normalize(u.sunDir.xyz);
            float sunI   = u.sunDir.w;
            float Rbot   = u.bottomRadius;
            float Rtop   = u.topRadius;

            vec3  viewPos = u.cameraPos.xyz;
            float vpLen   = length(viewPos);
            if (vpLen < 1.0)
                viewPos = vec3(0.0, Rbot + 1.0, 0.0);
            else if (vpLen < Rbot + 1.0)
                viewPos = viewPos * ((Rbot + 1.0) / vpLen);

            float camHeight = length(viewPos);

            // shit way to avoid fixing depth :(
            //float depth = 0.0f;
            // this down here causes a super dull blue atmo :( harass the goat claude to fix it ig
            // probabfly because whatever wrote to  it (litmeshpipelinepass) wasn't configured to read and write (neither is this, fuck) the depth for infinite far plane
            // along with the reversed Z buffer.
            float depth = texture(sceneDepth, screenUV).r;
            if (depth > 0.0)
            {
                vec3  surface   = texture(sceneColor, screenUV).rgb;
                float sceneDist = depthToViewDist(depth, ndc);

                vec3 scatter, transmit;
                sampleAerialPerspective(screenUV, sceneDist, scatter, transmit);

                outColor = vec4(surface * transmit + scatter, 1.0);
                return;
            }

            vec2 atmoHit = raySphereIntersect(viewPos, rd, Rtop);

            float camAlt        = camHeight - Rbot;
            float atmoThickness = Rtop - Rbot;

            if (camAlt < atmoThickness - 1.0)
            {
                // Inside atmosphere → SkyView LUT
                vec3 col = sampleSkyView(rd, viewPos, sunDir, camHeight, Rbot);
                col += sunDisk(rd, sunDir, sunI, viewPos, Rbot, Rtop);
                col  = 1.0 - exp(-col);

                float cosSky   = dot(normalize(viewPos), rd);
                vec3  skyTrans = sampleTransmittance(transmittanceLUT, camHeight,
                                                     cosSky, Rbot, Rtop);
                float atmAlpha = 1.0 - dot(skyTrans, vec3(0.2126, 0.7152, 0.0722));

                outColor = vec4(col, atmAlpha);
                return;
            }

            // Above atmosphere → full raymarch
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

            bool groundBlocking = (gndHit.x > 0.0 && gndHit.x < gndHit.y);
            if (!groundBlocking)
                col += sunDisk(rd, sunDir, sunI, viewPos, Rbot, Rtop);


            col = 1.0 - exp(-col);
            float atmAlpha = 1.0 - dot(transmittance, vec3(0.2126, 0.7152, 0.0722));
            outColor = vec4(col, atmAlpha);
        }
    }
}
