// Sun.shader : Standalone sun disc + halo.
// Fullscreen triangle at z=0.9999 (sky depth). Discards pixels outside
// the halo cone. Tinted by a short Rayleigh/Mie/Ozone transmittance march
// using Hillaire 2020 constants so the disc matches the atmosphere exactly.
// Works standalone (moon scene, no atmosphere required).
//
// set=0  bind=0  UBO  SunUBO

Shader "SF/Sun"
{
    VertexShader
    {
        #version 450
        void main()
        {
            vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(uv * 2.0 - 1.0, 0.9999, 1.0);
        }
    }

    FragmentShader
    {
        #version 450
        layout(location = 0) out vec4 outColor;

        layout(set = 0, binding = 0) uniform SunUBO
        {
            mat4  invProj;
            mat4  invView;
            vec4  sunDir;           // .xyz = toward sun (world, unit), .w = intensity
            vec4  sunColor;         // .xyz = tint, .w = unused
            vec2  screenSize;
            float discHalfAngleCos;
            float haloHalfAngleCos;
            float haloStrength;
            float bloomStrength;
            vec2  _pad;
        } u;

        //  Hillaire 2020 constants (identical to atmosphere shaders) 
        const vec3  kRayS  = vec3(5.802e-6, 13.558e-6, 33.1e-6);
        const float kRayH  = 8000.0;
        const float kMieS  = 3.996e-6;
        const float kMieA  = 4.40e-6;
        const float kMieH  = 1200.0;
        const vec3  kOzA   = vec3(0.650e-6, 1.881e-6, 0.085e-6);
        const float kOzAlt = 25000.0;
        const float kOzExt = 15000.0;
        const float Rbot   = 6360000.0;
        const float Rtop   = 6460000.0;
        const int   LS     = 8;

        vec2 RaySphere(vec3 centre, float r, vec3 ro, vec3 rd)
        {
            vec3  f = ro - centre;
            float b = dot(rd, f);
            float d = b*b - dot(f,f) + r*r;
            if (d < 0.0) return vec2(1e20, -1e20);
            float s = sqrt(d);
            return vec2(-b - s, -b + s);
        }

        // Short transmittance march from posSI toward sunDir.
        // Returns RGB transmittance * horizon shadow.
        vec3 SunTransmittance(vec3 posSI, vec3 sunDir)
        {
            vec3  planSI = vec3(0.0, -Rbot, 0.0);
            vec2  lh     = RaySphere(planSI, Rtop, posSI, sunDir);
            float ll     = max(lh.y, 0.0);
            if (ll <= 0.0) return vec3(0.0);

            float ls  = ll / float(LS);
            vec3  T   = vec3(1.0);
            vec3  pos = posSI;

            for (int j = 0; j < LS; j++)
            {
                pos += ls * sunDir;
                float la = max(length(pos - planSI) - Rbot, 0.0);
                float dR = exp(-la / kRayH);
                float dM = exp(-la / kMieH);
                float dO = max(0.0, 1.0 - abs(la - kOzAlt) / kOzExt);
                // Extinction = Rayleigh scatter + Mie (scatter+absorb) + Ozone absorb
                vec3 ext = kRayS * dR
                         + vec3(kMieS + kMieA) * dM
                         + kOzA * dO;
                T *= exp(-ext * ls);
            }

            // Horizon shadow : disc vanishes when sun is below ground
            vec3  rel  = posSI - planSI;
            float dist = length(rel);
            float cz   = dot(rel / dist, sunDir);
            float sinH = clamp(Rbot / dist, 0.0, 1.0);
            float cosH = -sqrt(max(1.0 - sinH*sinH, 0.0));
            return T * smoothstep(cosH - sinH*0.01, cosH + sinH*0.01, cz);
        }

        void main()
        {
            // Reconstruct view ray
            vec2 ndc = vec2(
                (gl_FragCoord.x / u.screenSize.x) * 2.0 - 1.0,
                (gl_FragCoord.y / u.screenSize.y) * 2.0 - 1.0
            );
            vec4 vp  = u.invProj * vec4(ndc, 1.0, 1.0);
            vec3 rd  = normalize((u.invView * vec4(vp.xyz / vp.w, 0.0)).xyz);

            vec3  sunDir = normalize(u.sunDir.xyz);
            float cosV   = dot(rd, sunDir);

            if (cosV < u.haloHalfAngleCos) discard;

            // Camera world position from invView translation column
            vec3 camPos = vec3(u.invView[3]);

            // Transmittance: white at zenith, orange/red at horizon, 0 at night
            vec3 trans = SunTransmittance(camPos, sunDir);
            if (dot(trans, vec3(1.0/3.0)) < 0.001) discard;

            // Disc + halo masks
            float discMask = smoothstep(u.discHalfAngleCos - 5e-5, u.discHalfAngleCos, cosV);
            float haloFade = smoothstep(u.haloHalfAngleCos, u.discHalfAngleCos, cosV);
            float haloMask = haloFade * (1.0 - discMask) * u.haloStrength;

            float brightness = u.sunDir.w * (discMask * (1.0 + u.bloomStrength) + haloMask);
            vec3  col        = u.sunColor.rgb * trans * brightness;

            // Reinhard : matches atmosphere exposure
            col = (col * 2.0) / (1.0 + col * 2.0);

            float alpha = max(discMask, haloMask * 0.5);
            outColor = vec4(col * alpha, alpha);
        }
    }
}
