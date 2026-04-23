// TransmittanceLUT.shader
// Baked once at startup. 256×64 RGBA16F.
// Stores T(r, cosZenith)  transmittance from altitude r toward the sun.
//
// UV convention (matches the sampler in Atmosphere.shader):
//   u  → height remapped linearly: u=0 at ground, u=1 at atmosphere top
//   v  → cosZenith remapped: v=0 → cos=-1 (straight down), v=1 → cos=+1 (straight up)
//
// Coefficients match Atmosphere.shader exactly so the transmittance is consistent.
//
// set=0 bind=0  transmittanceLUT (rgba16f storage image, writeonly)

Shader "SF/Atmosphere/TransmittanceLUT"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 8, local_size_y = 8) in;

        layout(rgba16f, set = 0, binding = 0) uniform writeonly image2D transmittanceLUT;

        //  Must match Atmosphere.shader constants exactly 
        const vec3  RAY_BETA          = vec3(5.5e-6, 13.0e-6, 22.4e-6);
        const vec3  MIE_BETA          = vec3(21e-6);
        const vec3  ABSORPTION_BETA   = vec3(2.04e-5, 4.97e-5, 1.95e-6);
        const float HEIGHT_RAY        = 8e3;
        const float HEIGHT_MIE        = 1.2e3;
        const float HEIGHT_ABSORPTION = 30e3;
        const float ABSORPTION_FALLOFF= 4e3;
        const float BOTTOM_RADIUS     = 6371000.0;
        const float TOP_RADIUS        = 6471000.0;
        const int   STEPS             = 40;

        //  Decode UV → (height, cosZenith) 
        void uvToParams(vec2 uv, out float height, out float cosZenith)
        {
            height    = mix(BOTTOM_RADIUS, TOP_RADIUS, uv.x);
            cosZenith = uv.y * 2.0 - 1.0;
        }

        //  Distance from (height, cosZenith) to atmosphere top 
        float distToAtmTop(float h, float cosZ)
        {
            float disc = h*h*(cosZ*cosZ - 1.0) + TOP_RADIUS*TOP_RADIUS;
            return max(0.0, -h*cosZ + sqrt(max(0.0, disc)));
        }

        void main()
        {
            ivec2 size  = imageSize(transmittanceLUT);
            ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
            if (any(greaterThanEqual(coord, size))) return;

            // Texel-centre UV
            vec2  uv       = (vec2(coord) + 0.5) / vec2(size);
            float h, cosZ;
            uvToParams(uv, h, cosZ);

            float dist = distToAtmTop(h, cosZ);
            float dt   = dist / float(STEPS);

            vec3 opticalDepth = vec3(0.0);
            for (int i = 0; i < STEPS; i++)
            {
                float t        = (float(i) + 0.5) * dt;
                // Altitude at sample point along the ray
                float altitude = sqrt(h*h + t*t + 2.0*h*cosZ*t) - BOTTOM_RADIUS;
                altitude = max(altitude, 0.0);

                float rhoRay = exp(-altitude / HEIGHT_RAY);
                float rhoMie = exp(-altitude / HEIGHT_MIE);
                float denom  = (HEIGHT_ABSORPTION - altitude) / ABSORPTION_FALLOFF;
                float rhoOz  = (1.0 / (denom*denom + 1.0)) * rhoRay;

                opticalDepth += vec3(rhoRay, rhoMie, rhoOz) * dt;
            }

            vec3 transmittance = exp(
                - RAY_BETA        * opticalDepth.x
                - MIE_BETA        * opticalDepth.y
                - ABSORPTION_BETA * opticalDepth.z);

            imageStore(transmittanceLUT, coord, vec4(transmittance, 1.0));
        }
    }
}
