// MultiScatterLUT.shader
// Bakes order-2+ multiple scattering into a 32x32 LUT (Hillaire 2020).
// Reads transmittanceLUT as input  must be baked first.
//
// UV convention (shared with Atmosphere.shader):
//   x = (height - bottomRadius) / (topRadius - bottomRadius)
//   y = cosSun * 0.5 + 0.5
//
// set=0 bind=0  transmittanceLUT  (sampler2D, 256x64  RGBA16F, read)
// set=0 bind=1  multiScatterLUT   (image2D,   32x32   RGBA16F, write)

Shader "SF/Atmosphere/MultiScatterLUT"
{
    ComputeShader
    {
        #version 450
        // One workgroup per texel. Z dimension = 64 rays for sphere integration.
        layout(local_size_x = 1, local_size_y = 1, local_size_z = 64) in;

        layout(set = 0, binding = 0) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 1, rgba16f) uniform writeonly image2D multiScatterLUT;

        //  Constants  must match Atmosphere.shader exactly 
        const vec3  RAY_BETA          = vec3(5.5e-6, 13.0e-6, 22.4e-6);
        const vec3  MIE_BETA          = vec3(21e-6);
        const vec3  ABSORPTION_BETA   = vec3(2.04e-5, 4.97e-5, 1.95e-6);
        const float HEIGHT_RAY        = 8e3;
        const float HEIGHT_MIE        = 1.2e3;
        const float HEIGHT_ABSORPTION = 30e3;
        const float ABSORPTION_FALLOFF= 4e3;
        const float BOTTOM_RADIUS     = 6371000.0;
        const float TOP_RADIUS        = 6471000.0;
        const float PI                = 3.14159265358979;
        const int   STEPS             = 20;
        const int   SPHERE_SAMPLES    = 64; // must equal local_size_z

        //  Shared memory  one slot per Z invocation 
        shared vec3 s_fms[SPHERE_SAMPLES];
        shared vec3 s_lms[SPHERE_SAMPLES];

        //  Helpers 
        vec3 sampleTransmittance(float height, float cosSun)
        {
            float x = clamp((height - BOTTOM_RADIUS) / (TOP_RADIUS - BOTTOM_RADIUS), 0.0, 1.0);
            float y = clamp(cosSun * 0.5 + 0.5, 0.0, 1.0);
            return textureLod(transmittanceLUT, vec2(x, y), 0.0).rgb;
        }

        // Distance along ray from height h, direction cosine mu, to sphere R.
        // Returns -1 if no intersection in front of ray.
        float rayToSphere(float h, float mu, float R)
        {
            float disc = h * h * (mu * mu - 1.0) + R * R;
            if (disc < 0.0) return -1.0;
            return max(0.0, -h * mu + sqrt(max(0.0, disc)));
        }

        // Fibonacci lattice  evenly distributes SPHERE_SAMPLES rays over the unit sphere
        vec3 fibonacciDir(int i)
        {
            float phi   = acos(1.0 - 2.0 * (float(i) + 0.5) / float(SPHERE_SAMPLES));
            float theta = PI * (1.0 + sqrt(5.0)) * float(i);
            return normalize(vec3(sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta)));
        }

        //  Main 
        void main()
        {
            ivec2 lutSize = imageSize(multiScatterLUT);
            ivec2 coord   = ivec2(gl_WorkGroupID.xy); // one workgroup per output texel
            int   rayIdx  = int(gl_LocalInvocationID.z);

            // Decode texel → physical params
            vec2  uv     = (vec2(coord) + 0.5) / vec2(lutSize);
            float h      = mix(BOTTOM_RADIUS, TOP_RADIUS, uv.x);
            float cosSun = uv.y * 2.0 - 1.0;
            // Sun points along XZ plane at this zenith angle  Y is up
            float sinSun = sqrt(max(0.0, 1.0 - cosSun * cosSun));
            vec3  sunDir = vec3(sinSun, cosSun, 0.0);
            vec3  pos    = vec3(0.0, h, 0.0); // camera sits on Y axis at height h

            // This invocation marches one ray in the sphere
            vec3 rayDir  = fibonacciDir(rayIdx);
            float mu     = rayDir.y; // cosine of ray with zenith (planet centred)

            float tGround = rayToSphere(h, mu, BOTTOM_RADIUS);
            float tTop    = rayToSphere(h, mu, TOP_RADIUS);
            float tMax    = (tGround > 0.0) ? tGround : max(tTop, 0.0);

            vec3 fms    = vec3(0.0); // multiple-scatter factor (geometric series base)
            vec3 lms    = vec3(0.0); // multiple-scatter luminance
            vec3 T_view = vec3(1.0); // transmittance accumulated along this ray

            if (tMax > 0.0)
            {
                float dt = tMax / float(STEPS);

                for (int i = 0; i < STEPS; i++)
                {
                    float t   = (float(i) + 0.5) * dt;
                    vec3  p   = pos + rayDir * t;
                    float alt = max(length(p) - BOTTOM_RADIUS, 0.0);

                    // Density
                    float rho_ray = exp(-alt / HEIGHT_RAY);
                    float rho_mie = exp(-alt / HEIGHT_MIE);
                    float denom   = (HEIGHT_ABSORPTION - alt) / ABSORPTION_FALLOFF;
                    float rho_oz  = (1.0 / (denom * denom + 1.0)) * rho_ray;

                    vec3 sigma_s = RAY_BETA * rho_ray + MIE_BETA * rho_mie;
                    vec3 sigma_t = sigma_s + ABSORPTION_BETA * rho_oz;

                    // Step transmittance
                    vec3 T_step = exp(-sigma_t * dt);

                    // Sun transmittance at this point
                    float cosSunHere = dot(normalize(p), sunDir);
                    vec3  T_sun      = sampleTransmittance(length(p), cosSunHere);

                    // Isotropic phase (1/4π) baked in  uniform sphere average
                    const float ISO = 1.0 / (4.0 * PI);
                    vec3 S    = ISO * sigma_s * T_sun;
                    // Analytic single-step luminance integral
                    vec3 Sint = (S - S * T_step) / max(sigma_t, vec3(1e-7));

                    fms    += T_view * sigma_s * ISO * dt;
                    lms    += T_view * Sint;
                    T_view *= T_step;
                }

                // Ground albedo bounce  Lambertian, albedo 0.3
                if (tGround > 0.0)
                {
                    vec3  gndPos    = pos + rayDir * tGround;
                    float cosGndSun = dot(normalize(gndPos), sunDir);
                    vec3  T_sun_gnd = sampleTransmittance(BOTTOM_RADIUS, cosGndSun);
                    lms += T_view * (0.3 / PI) * max(cosGndSun, 0.0) * T_sun_gnd;
                }
            }

            // Write this ray's contribution into shared memory
            s_fms[rayIdx] = fms;
            s_lms[rayIdx] = lms;
            barrier();
            memoryBarrierShared();

            // Only invocation 0 reduces and writes output
            if (rayIdx == 0)
            {
                vec3 totalFms = vec3(0.0);
                vec3 totalLms = vec3(0.0);
                for (int k = 0; k < SPHERE_SAMPLES; k++)
                {
                    totalFms += s_fms[k];
                    totalLms += s_lms[k];
                }
                // Average over sphere
                totalFms /= float(SPHERE_SAMPLES);
                totalLms /= float(SPHERE_SAMPLES);

                // Closed-form infinite geometric series: L_ms = lms / (1 - fms)
                vec3 ms = totalLms / max(vec3(1e-4), vec3(1.0) - totalFms);
                imageStore(multiScatterLUT, coord, vec4(ms, 1.0));
            }
        }
    }
}