Shader "SF/Atmosphere/SkyView"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

        layout(set = 0, binding = 0) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 1) uniform sampler2D multiScatterLUT;
        layout(rgba16f, set = 0, binding = 2) uniform writeonly image2D skyViewLUT;

        layout(push_constant) uniform PC {
            vec4  sunDir;        // .xyz = toward sun (unit), .w = intensity
            float cameraHeight;  // metres above planet surface
            float bottomRadius;  // metres
            float topRadius;     // metres
            float _pad;
        } pc;

        #import "Atmosphere.si"

        void main()
        {
            ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
            ivec2 size  = imageSize(skyViewLUT);
            if (coord.x >= size.x || coord.y >= size.y) return;

            vec2 uv = (vec2(coord) + 0.5) / vec2(size);

            float phi   = uv.x * 2.0 * PI;           // [0, 2PI]
            float theta = skyViewDecodeV(uv.y);       // [-PI/2, PI/2]

            vec3 rd = vec3(cos(theta) * sin(phi),
                           sin(theta),
                           cos(theta) * cos(phi));

            float h       = pc.bottomRadius + max(pc.cameraHeight, 1.0);
            vec3  viewPos = vec3(0.0, h, 0.0);

            vec2  gndHit  = raySphereIntersect(viewPos, rd, pc.bottomRadius);
            float maxDist = 1e12;
            if (gndHit.x > 0.0 && gndHit.x < gndHit.y)
                maxDist = gndHit.x;

            vec3 sunDir = normalize(pc.sunDir.xyz);
            vec3 transmittance;
            vec3 col = calculateScattering(
                viewPos, rd, maxDist,
                sunDir, vec3(pc.sunDir.w),
                pc.bottomRadius, pc.topRadius,
                transmittanceLUT, multiScatterLUT,
                transmittance);

            imageStore(skyViewLUT, coord, vec4(col, 1.0));
        }
    }
}
