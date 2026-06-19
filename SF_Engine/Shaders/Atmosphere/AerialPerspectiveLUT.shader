Shader "SF/Atmosphere/AerialPerspectiveLUT"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
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

        layout(rgba16f, set = 0, binding = 1) uniform writeonly image3D aerialPerspColorRGBTransR;
        layout(rg16f,   set = 0, binding = 2) uniform writeonly image3D aerialPerspTransGB;
        layout(r32f,    set = 0, binding = 3) uniform writeonly image2D aerialPerspRange;

        layout(set = 0, binding = 4) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 5) uniform sampler2D multiScatterLUT;

        #import "Atmosphere.si"

        // Reconstruct a world-space ray direction from NDC (x,y) and the UBO matrices.
        vec3 ndcToRayDir(vec2 ndc)
        {
            vec4 vp = u.invProj * vec4(ndc, 1.0, 1.0);
            return normalize((u.invView * vec4(vp.xyz / vp.w, 0.0)).xyz);
        }

        float sliceDepth(float p, float maxDist)
        {
            return p * p * maxDist;
        }

        void main()
        {
            ivec3 lutSize = imageSize(aerialPerspColorRGBTransR);
            ivec2 coord2  = ivec2(gl_GlobalInvocationID.xy);
            if (any(greaterThanEqual(coord2, lutSize.xy))) return;

            int sliceCount = lutSize.z;
            vec2 uv  = (vec2(coord2) + 0.5) / vec2(lutSize.xy);
            vec2 ndc = uv * 2.0 - 1.0;

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

            vec3 rd = ndcToRayDir(ndc);

            vec2 atmoHit  = raySphereIntersect(viewPos, rd, Rtop);
            vec2 gndHit   = raySphereIntersect(viewPos, rd, Rbot);

            // Ray misses the atmosphere entirely — fill with no-op and bail.
            if (atmoHit.y < 0.0)
            {
                imageStore(aerialPerspRange, coord2, vec4(-1.0));
                for (int z = 0; z < sliceCount; z++)
                {
                    ivec3 c = ivec3(coord2, z);
                    imageStore(aerialPerspColorRGBTransR, c, vec4(0.0, 0.0, 0.0, 1.0));
                    imageStore(aerialPerspTransGB,         c, vec4(1.0, 1.0, 0.0, 0.0));
                }
                return;
            }

            float startDist = max(atmoHit.x, 0.0);          // camera may be inside atmo
            float endDist   = atmoHit.y;

            // Clamp to ground if the ray hits the planet surface.
            if (gndHit.x > 0.0 && gndHit.x < gndHit.y)
                endDist = min(endDist, gndHit.x);

            float distToTravel = max(endDist - startDist, 0.0);

            // Store range for compositor depth → slice mapping.
            imageStore(aerialPerspRange, coord2, vec4(distToTravel));
            float mu     = dot(rd, sunDir);
            float mumu   = mu * mu;
            float gg     = G * G;
            float phaseR = 3.0 / (16.0 * PI) * (1.0 + mumu);
            float phaseM = 3.0 / (8.0  * PI)
                         * ((1.0 - gg) * (mumu + 1.0))
                         / (pow(1.0 + gg - 2.0 * mu * G, 1.5) * (2.0 + gg));

            float camHeight = length(viewPos);
            bool  nonLinear = (camHeight < Rtop);

            // State accumulated from the camera outward:
            vec3 totalScatter   = vec3(0.0);
            vec3 totalTransmit  = vec3(1.0);

            // distanceTravelled tracks position along [0, distToTravel].
            float distanceTravelled = 0.0;

           
            for (int i = 0; i < sliceCount; i++)
            {
                float sliceFrac = float(i + 1) / float(sliceCount);
                float targetDist;
                if (nonLinear)
                    targetDist = sliceDepth(sliceFrac, distToTravel);  // quadratic
                else
                    targetDist = sliceFrac * distToTravel;              // linear

                float stepSize = targetDist - distanceTravelled;

                float tMid   = startDist + distanceTravelled + stepSize * 0.5;
                vec3  posI   = viewPos + rd * tMid;
                float rI     = length(posI);
                float altI   = max(rI - Rbot, 0.0);

                vec3  sigma_s = vec3(0.0);   // scattering coefficient at this point
                vec3  sigma_t = vec3(0.0);   // extinction coefficient at this point

                float rhoRay = exp(-altI / HEIGHT_RAY);
                float rhoMie = exp(-altI / HEIGHT_MIE);
                float ozDen  = (HEIGHT_ABSORPTION - altI) / ABSORPTION_FALLOFF;
                float rhoOz  = (1.0 / (ozDen * ozDen + 1.0)) * rhoRay;

                sigma_s  = RAY_BETA * rhoRay + MIE_BETA * rhoMie;
                sigma_t  = sigma_s + ABSORPTION_BETA * rhoOz;

                vec3 T_step  = exp(-sigma_t * stepSize);
                vec3 weight  = (vec3(1.0) - T_step) / max(sigma_t, vec3(1e-7));

                float cosSunI = dot(posI / rI, sunDir);
                vec3 sunTrans = sampleTransmittance(transmittanceLUT, rI, cosSunI, Rbot, Rtop);

                vec3 singleScatter = totalTransmit * weight * sunTrans
                                   * (phaseR * RAY_BETA * rhoRay
                                    + phaseM * MIE_BETA * rhoMie);

                vec3 ms     = sampleMultiScatter(multiScatterLUT, rI, cosSunI, Rbot, Rtop);
                vec3 multiS = totalTransmit * weight * ms * sigma_s;

                totalScatter  += (singleScatter + multiS) * sunI;
                totalTransmit *= T_step;

                distanceTravelled = targetDist;

                ivec3 sliceCoord = ivec3(coord2, i);
                imageStore(aerialPerspColorRGBTransR, sliceCoord,
                           vec4(totalScatter, totalTransmit.r));
                imageStore(aerialPerspTransGB, sliceCoord,
                           vec4(totalTransmit.gb, 0.0, 0.0));
            }
        }
    }
}
