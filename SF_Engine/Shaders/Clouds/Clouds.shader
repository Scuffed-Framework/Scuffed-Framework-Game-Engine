Shader "SF/Clouds/Clouds"
{
    VertexShader
    {
        #version 450
        void main()
        {
            // Fullscreen triangle  (same pattern as Atmosphere.shader)
            vec2 uv     = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
            gl_Position = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
        }
    }

    FragmentShader
    {
        #version 450
        layout(location = 0) out vec4 outColor; // premultiplied alpha

        layout(set = 0, binding = 0) uniform AtmoUBO
        {
            mat4  invProj;
            mat4  invView;
            vec4  cameraPos;         // .xyz = planet-relative world metres
            vec4  planetPos;         // always vec4(0)
            vec4  sunDir;            // .xyz = toward sun (unit),  .w = intensity
            float bottomRadius;
            float topRadius;
            float renderUnitRadius;
            float _p0;
            vec2  screenSize;
            vec2  _p1;
        } atmo;

        layout(set = 0, binding = 1) uniform CloudUBO
        {
            float cloudBottomRadius;  // params.bottomRadius + minAlt
            float cloudTopRadius;     // params.bottomRadius + maxAlt
            float stepCount;
            float lightStepCount;     // unused (replaced by precomputed accum)

            float cloudDensityScale;
            float cloudCoverage;
            float windSpeed;
            float cloudType;          // global type bias

            float extinctionCoeff;    // sigma_e  (m^-1)
            float scatteringAlbedo;   // sigma_s / sigma_e
            float time;
            float _pad0;

            vec3  voxelBoundsMin;
            float sdfRangeMetres;

            vec3  voxelBoundsMax;
            float _pad1;
        } c;

        layout(set = 0, binding = 2) uniform sampler2D blueNoise;       // 128×128 2D
        layout(set = 0, binding = 3) uniform sampler3D worleyNoise;  // 128³ RGBA8
        layout(set = 0, binding = 4) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 5) uniform sampler2D multiScatterLUT;


        void main()
        {
            return;
        }
    }
}
