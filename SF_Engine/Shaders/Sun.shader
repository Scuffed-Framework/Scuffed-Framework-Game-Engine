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
        #import "Atmosphere/Atmosphere.si"

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

        layout(set = 0, binding = 1) uniform sampler2D transmittanceLUT;

        void main()
        {
            vec2 ndc = vec2(
                (gl_FragCoord.x / u.screenSize.x) * 2.0 - 1.0,
                (gl_FragCoord.y / u.screenSize.y) * 2.0 - 1.0
            );
            vec4 vp  = u.invProj * vec4(ndc, 1.0, 1.0);
            vec3 rd  = normalize((u.invView * vec4(vp.xyz / vp.w, 0.0)).xyz);

            vec3  sunDir = normalize(u.sunDir.xyz);
            float cosV   = dot(rd, sunDir);

            if (cosV < u.discHalfAngleCos) discard;

            // Use sunDir.y (the disc centre) for the discard check only.
            // This keeps the full disc visible even when the bottom half is
            // geometrically below the horizon line.
            float camHeight  = BOTTOM_RADIUS + 2.0;
            float cosSunCentre = sunDir.y;

            vec3 transCentre = sampleTransmittance(
                transmittanceLUT, camHeight, cosSunCentre,
                BOTTOM_RADIUS, TOP_RADIUS);

            if (dot(transCentre, vec3(1.0 / 3.0)) < 0.001) discard;

            // Each pixel's ray has a slightly different zenith angle to the sun.
            // Lower pixels graze more atmosphere → more Rayleigh scattering of
            // blue/green → redder colour. This is what creates the natural
            // yellow-top / red-bottom gradient without any hardcoded ramp.
            //
            // CLAMP to sunDir.y so we never query the LUT below the sun centre's
            // horizon, below that the transmittance collapses to zero and we'd
            // get black pixels chopping the bottom of the disc.
            // The clamp floor is sunDir.y which is where the disc centre sits;
            // anything below that just gets the same deep-red as the centre row.
            float cosZenithPixel = max(rd.y, sunDir.y);

            vec3 trans = sampleTransmittance(
                transmittanceLUT, camHeight, cosZenithPixel,
                BOTTOM_RADIUS, TOP_RADIUS);

            float brightness = u.sunDir.w * (1.0 + u.bloomStrength);
            vec3  col        = u.sunColor.rgb * trans * brightness;

            // Per-channel Reinhard preserves hue under high exposure
            col = col / (1.0 + col);

            outColor = vec4(col, 1.0);
        }
    }
}
