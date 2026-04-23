// CloudRaymarch.shader
// Physically-based single-scattering volumetric cloud raymarch pass.
// Renders into a half-resolution R16G16B16A16_SFLOAT offscreen buffer.
//   RGB  = in-scattered light (premultiplied by transmittance)
//   A    = transmittance (1 = fully transparent, 0 = fully opaque)
//
// Adapted from robobo1221's "Real-time PBR Volumetric Clouds" (shadertoy.com)
// and GPU Pro 7 "Real-Time Volumetric Cloudscapes" (Andrew Schneider).
//
// Descriptor layout (set = 0):
//   bind 0  : sampler3D  basicNoise    (Worley-FBM 3D, 128^3)
//   bind 1  : sampler3D  detailNoise   (curl  noise 3D,  32^3)
//   bind 2  : sampler2D  coverage      (coverage LUT, 512x512)
//   bind 3  : sampler2D  blueNoise     (blue-noise dither, 128x128, REPEAT)
//   bind 4  : sampler2D  shadowLUT     (cloud self-shadow, 256x256)
//   bind 5  : uniform    CloudFrameUBO

#import "Clouds/CloudNoise"

Shader "SF/Clouds/CloudRaymarch"
{
    VertexShader
    {
        #version 450

        void main()
        {
            // Full-screen triangle (no VBO needed)
            vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(uv * 2.0 - 1.0, 0.9999, 1.0);
        }
    }

    FragmentShader
    {
        #version 450
        layout(location = 0) out vec4 outCloud;   // RGB=scattering, A=transmittance

        // ------------------------------------------------------------------
        // Samplers
        // ------------------------------------------------------------------
        layout(set = 0, binding = 0) uniform sampler3D basicNoise;
        layout(set = 0, binding = 1) uniform sampler3D detailNoise;
        layout(set = 0, binding = 2) uniform sampler2D coverage;
        layout(set = 0, binding = 3) uniform sampler2D blueNoise;
        layout(set = 0, binding = 4) uniform sampler2D shadowLUT;

        // ------------------------------------------------------------------
        // UBO
        // ------------------------------------------------------------------
        layout(set = 0, binding = 5) uniform CloudFrameUBO
        {
            mat4  invViewProj;          // clip -> world
            vec3  cameraPos;            // world-space camera (metres)
            float time;                 // elapsed time (seconds)
            vec3  sunDir;               // unit vector toward sun
            float sunIntensity;         // multiplier
            vec3  sunColor;             // pre-tinted sun colour (from atmosphere)
            float cloudSpeed;           // horizontal wind speed
            float cloudBot;             // cloud layer bottom altitude (metres)
            float cloudTop;             // cloud layer top  altitude  (metres)
            float cloudDensity;         // optical depth scale
            float earthRadius;          // metres (default 6371000)
            vec2  screenSize;           // pixels of this half-res buffer
            float windAngle;            // wind direction (radians)
            float _pad0;
            vec2  windOffset;           // accumulated wind displacement (metres XZ)
            vec2  _pad1;
        } u;

        // ------------------------------------------------------------------
        // Constants
        // ------------------------------------------------------------------
        const float PI     = 3.14159265359;
        const float TWO_PI = 6.28318530718;
        const float INV_PI = 0.31830988618;

        // Atmospheric scatter coefficients (Hillaire 2020, low-precision)
        const vec3  kRayleigh = vec3(5.5e-6, 13.0e-6, 22.4e-6);
        const vec3  kMie      = vec3(3.996e-6);

        // Raymarch quality
        const int   CLOUD_STEPS        = 32;   // primary march steps through cloud slab
        const int   SHADOW_STEPS       = 8;    // secondary march toward sun per step
        const float TRANSMITTANCE_CLIP = 0.01; // early-out threshold

        // ------------------------------------------------------------------
        // Henyey-Greenstein phase function
        // ------------------------------------------------------------------
        float hgPhase(float cosTheta, float g)
        {
            float g2    = g * g;
            float denom = 1.0 + g2 - 2.0 * g * cosTheta;
            return (1.0 - g2) / (4.0 * PI * denom * sqrt(denom));
        }

        // Dual-lobe phase (forward + backward scatter)
        float cloudPhase(float cosTheta)
        {
            float fwd  = hgPhase(cosTheta,  0.6);
            float bwd  = hgPhase(cosTheta, -0.2);
            return mix(bwd, fwd, 0.65);
        }

        // ------------------------------------------------------------------
        // Powder sugar effect (Beer-Lambert + thin-slab enhancement)
        // ------------------------------------------------------------------
        float powder(float od)
        {
            return 1.0 - exp(-od * 2.0);
        }

        // ------------------------------------------------------------------
        // Ray-sphere intersection  (returns near, far; negative = no hit)
        // ------------------------------------------------------------------
        vec2 raySphere(vec3 origin, vec3 dir, float radius)
        {
            float a = dot(dir, dir);
            float b = 2.0 * dot(origin, dir);
            float c = dot(origin, origin) - radius * radius;
            float d = b * b - 4.0 * a * c;
            if (d < 0.0) return vec2(-1.0);
            float sqrtD = sqrt(d);
            return vec2((-b - sqrtD) / (2.0 * a),
                        (-b + sqrtD) / (2.0 * a));
        }

        // ------------------------------------------------------------------
        // Sample the cloud density at world point p
        // ------------------------------------------------------------------
      // ------------------------------------------------------------------
        float sampleDensity(vec3 p)
        {
            // Spherical height (handles curvature at large scale)
            float r    = length(p + vec3(0.0, u.earthRadius, 0.0));
            float alt  = r - u.earthRadius;

            if (alt < u.cloudBot || alt > u.cloudTop)
                return 0.0;

            // Height fraction [0,1]
            float hFrac = (alt - u.cloudBot) / (u.cloudTop - u.cloudBot);

            // Wind-displaced texture coordinate.
            // IMPORTANT: subtract camera's XZ tile origin before scaling so that
            // the coordinate stays small regardless of world-space distance from
            // origin.  We tile every TILE_M metres; fract() keeps us in [0,1).
            const float TILE_M  = 50000.0;   // must match coverage tiling below
            const float INV_TILE = 1.0 / TILE_M;
            vec3 pTiled   = vec3(fract(p.x * INV_TILE), (p.y - u.cloudBot) / max(u.cloudTop - u.cloudBot, 1.0), fract(p.z * INV_TILE));
            vec3 movement = vec3(u.windOffset.x, 0.0, u.windOffset.y) * INV_TILE;
            vec3 coord    = pTiled * 0.5 + movement;   // stays in a small float range

            // Base shape LUT
            float baseNoise = texture(basicNoise, coord).r;

            // Coverage map -- use the same tiled XZ so it doesn't alias at distance
            vec2 covUV = fract(vec2(pTiled.xz) + movement.xz * 0.1);
            float cov  = texture(coverage, covUV).r;

            // Remap: cloud exists where baseNoise exceeds (1 - coverage).
            // Add a generous bias so partial-coverage sky still shows clouds.
            float threshold = max(0.0, 0.6 - cov * 0.55);
            float cloud = clamp((baseNoise - threshold) / max(1.0 - threshold, 0.001), 0.0, 1.0);

            if (cloud < 0.001)
                return 0.0;

            // Height gradient: smooth rise at base, sharp fall at top.
            // NOTE: smoothstep requires edge0 < edge1.
            float gradBot = smoothstep(0.0,  0.15, hFrac);
            float gradTop = 1.0 - smoothstep(0.85, 1.0,  hFrac);
            cloud *= gradBot * gradTop;

            if (cloud < 0.001)
                return 0.0;

            // Detail erosion -- coord is already tiled, scale up for high-frequency detail
            float det   = texture(detailNoise, coord * 8.0 + movement * 0.5).a;
            float erode = mix(det, 1.0 - det, clamp(hFrac * 4.0, 0.0, 1.0));
            cloud = clamp(cloud - erode * 0.25, 0.0, 1.0);

            return cloud * u.cloudDensity;
        }


        // ------------------------------------------------------------------
        // Secondary march toward the sun for self-shadowing
        // ------------------------------------------------------------------
        float sampleSunVisibility(vec3 p)
        {
            const float slabThickness = max(u.cloudTop - u.cloudBot, 1.0);
            float rSteps = slabThickness / float(SHADOW_STEPS);

            vec3  inc    = u.sunDir * rSteps / max(abs(u.sunDir.y), 0.05);
            vec3  pos    = p + inc * 0.5;
            float shadow = 0.0;

            for (int i = 0; i < SHADOW_STEPS; i++, pos += inc)
                shadow += sampleDensity(pos);

            return exp2(-shadow * rSteps);
        }

        // ------------------------------------------------------------------
        // Approximate atmospheric in-scattering at cloud top
        // (Rayleigh + Mie, no beer-lambert, just sky ambient)
        // ------------------------------------------------------------------
        vec3 skyAmbient()
{
    float lDotU = clamp(u.sunDir.y, 0.0, 1.0);
    vec3 horizonCol = vec3(1.0, 0.55, 0.2);
    vec3 zenithCol  = vec3(0.3, 0.55, 1.0);
    return mix(horizonCol, zenithCol, lDotU) * 0.15;
}

        // ------------------------------------------------------------------
        // Main
        // ------------------------------------------------------------------
        void main()
        {
            // Reconstruct world-space ray direction from NDC
            vec2 ndc = vec2(
                (gl_FragCoord.x / u.screenSize.x) * 2.0 - 1.0,
                (gl_FragCoord.y / u.screenSize.y) * 2.0 - 1.0);

            vec4 worldFar = u.invViewProj * vec4(ndc, 1.0, 1.0);
            worldFar.xyz /= worldFar.w;
            vec3 rd = normalize(worldFar.xyz - u.cameraPos);

            // Blue-noise dither offset (temporal jitter per pixel)
            vec2 bnUV  = gl_FragCoord.xy / 128.0;
            float dither = texture(blueNoise, bnUV).r;

            // Intersect cloud shell (spherical model)
            vec3  origin = u.cameraPos + vec3(0.0, u.earthRadius, 0.0);
            float camR   = length(origin);
            float botR   = u.earthRadius + u.cloudBot;
            float topR   = u.earthRadius + u.cloudTop;

            vec2 botHit = raySphere(origin, rd, botR);
            vec2 topHit = raySphere(origin, rd, topR);

            float tStart, tEnd;

            if (camR < botR)
            {
                // Below layer: ray punches up through bottom then top
                tStart = botHit.y;
                tEnd   = topHit.y;
            }
            else if (camR > topR)
            {
                // Above layer: enter at top sphere near hit, exit at bottom sphere near hit
                // Both .x values are the near (closer) intersection of each sphere.
                // topHit.x < botHit.x when looking down through the slab.
                tStart = topHit.x;
                tEnd   = botHit.x > tStart ? botHit.x : topHit.y; // fallback: use top-sphere far hit for grazing rays
            }
            else
            {
                // Inside layer: start immediately, exit at top
                tStart = 0.0;
                tEnd   = topHit.y;
            }

 // Only skip rays that are aimed at the ground and we are below the layer.
 // Never cull downward rays when we are already inside or above the cloud slab.
 bool belowLayer = (camR < botR);
 if (tStart < 0.0 || tEnd <= tStart || (belowLayer && rd.y < -0.05))
 {
     outCloud = vec4(0.0, 0.0, 0.0, 1.0);
     return;
 }

            const float MAX_DIST = 200000.0;
            tEnd = min(tEnd, tStart + MAX_DIST);

            float stepLen = (tEnd - tStart) / float(CLOUD_STEPS);
            float t       = tStart + stepLen * dither;

            float cosTheta = dot(rd, u.sunDir);
            float phase    = cloudPhase(cosTheta);

            vec3  scattering    = vec3(0.0);
            float transmittance = 1.0;
            vec3  sky           = skyAmbient();

            for (int i = 0; i < CLOUD_STEPS; i++, t += stepLen)
            {
                vec3 p = u.cameraPos + rd * t;

                float od = sampleDensity(p) * stepLen;
                if (od <= 0.0)
                    continue;

                float sunVis  = sampleSunVisibility(p);
                float pwdr    = powder(od);

                // Attenuate sun at the very top of the cloud (hFrac near 1):
                // the top surface is unoccluded by definition so without this
                // it blows out to full brightness.
                float r2    = length(p + vec3(0.0, u.earthRadius, 0.0)) - u.earthRadius;
                float hF    = clamp((r2 - u.cloudBot) / max(u.cloudTop - u.cloudBot, 1.0), 0.0, 1.0);
                float topAtten = 1.0 - smoothstep(0.75, 1.0, hF);

                // sunIntensity is already in physical units; no extra 0.05 scale needed.
                vec3 sunLight = u.sunColor * sunVis * pwdr * phase * u.sunIntensity * topAtten;
                // Raise ambient so clouds are lit from all sides, not just the sun.
                vec3 skyLight = sky * 1.5;

                // Fixed: (1 - exp(-od)) is the correct single-step integral
                float scatter1 = 1.0 - exp(-od);

                scattering    += (sunLight + skyLight) * transmittance * scatter1;
                transmittance *= exp(-od);

                if (transmittance < TRANSMITTANCE_CLIP)
{
    transmittance = TRANSMITTANCE_CLIP;  // don't zero it out
    break;
}
            }

            outCloud = vec4(scattering, transmittance);
        }
    }
}
