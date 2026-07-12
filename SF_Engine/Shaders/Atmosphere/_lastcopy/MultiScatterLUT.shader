Shader "SF/Atmosphere/MultiScatterLUT"
{
    ComputeShader
    {
        #version 450
        const int   SPHERE_SAMPLES    = 64; // must equal local_size_z
        // One workgroup per texel. Z dimension = 64 rays for sphere integration.
        layout(local_size_x=1, local_size_y=1, local_size_z=SPHERE_SAMPLES) in;

        layout(set = 0, binding = 0) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 1, rgba16f) uniform writeonly image2D multiScatterLUT;

        #import "Atmosphere.si"

        shared vec3 s_fms[SPHERE_SAMPLES];
        shared vec3 s_lms[SPHERE_SAMPLES];


        float rayToSphere(float h, float mu, float R)
        {
            float disc = h * h * (mu * mu - 1.0) + R * R;
            if (disc < 0.0) return -1.0;
            float t = -h * mu + sqrt(max(0.0, disc));
            return t > 0.0 ? t : -1.0;  // don't clamp, preserve the "no hit" signal
        }

        vec3 fibonacciDir(int i)
        {
            float phi   = acos(1.0 - 2.0 * (float(i) + 0.5) / float(SPHERE_SAMPLES));
            float theta = PI * (1.0 + sqrt(5.0)) * float(i);
            return normalize(vec3(sin(phi) * cos(theta), cos(phi), sin(phi) * sin(theta)));
        }

        void main()
        {
            ivec2 lutSize = imageSize(multiScatterLUT);
            ivec2 coord   = ivec2(gl_WorkGroupID.xy); // one workgroup per output texel
            int   rayIdx  = int(gl_LocalInvocationID.z);

            vec2  uv     = (vec2(coord) + 0.5) / vec2(lutSize);
            float h      = mix(BOTTOM_RADIUS, TOP_RADIUS, uv.x);
            float cosSun = uv.y * 2.0 - 1.0;

            float sinSun = sqrt(max(0.0, 1.0 - cosSun * cosSun));
            vec3  sunDir = vec3(sinSun, cosSun, 0.0);
            vec3  pos    = vec3(0.0, h, 0.0); // camera sits on Y axis at height h

            vec3 rayDir  = fibonacciDir(rayIdx);
            float mu     = rayDir.y; // cosine of ray with zenith (planet centred)

            float tGround = rayToSphere(h, mu, BOTTOM_RADIUS);
            float tTop    = rayToSphere(h, mu, TOP_RADIUS);
            float tMax    = (tGround > 0.0) ? tGround : tTop; // tTop is -1 if no hit

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
                    vec3  T_sun      = sampleTransmittance(transmittanceLUT, alt, cosSunHere,
                                                          BOTTOM_RADIUS, TOP_RADIUS);

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
                    vec3  T_sun_gnd = sampleTransmittance(transmittanceLUT, BOTTOM_RADIUS, cosGndSun,
                                                          BOTTOM_RADIUS, TOP_RADIUS);
                    lms += T_view * (0.3 / PI) * max(cosGndSun, 0.0) * T_sun_gnd;
                }
            }

            s_fms[rayIdx] = fms;
            s_lms[rayIdx] = lms;
            
            memoryBarrierShared();
            barrier();

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