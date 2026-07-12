Shader "SF/Atmosphere/SkyView"
{
    ComputeShader
    {
        #version 450
        layout(local_size_x = 8, local_size_y = 8) in;   // FIX 1: was missing → defaulted to 1×1×1

        layout(set = 0, binding = 0) uniform sampler2D transmittanceLUT;
        layout(set = 0, binding = 1) uniform sampler2D multiScatterLUT;
        layout(rgba16f, set = 0, binding = 2) uniform writeonly image2D skyViewLUT;
        layout(set = 0, binding = 3) uniform SkyViewUBO {   // was push_constant
            vec4  sunDir;
            float cameraHeight;
            float bottomRadius;
            float topRadius;
            float _pad;
        } pc;

        #import "Atmosphere.si"

        void main()
        {
            ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
            ivec2 size  = imageSize(skyViewLUT);
            if (coord.x >= size.x || coord.y >= size.y) return;

            vec2 uv = (vec2(coord) + 0.5) / vec2(size);

            float h       = pc.bottomRadius + max(pc.cameraHeight, 1.0);
            vec3  viewPos = vec3(0.0, h, 0.0);
            vec3  up      = vec3(0.0, 1.0, 0.0);

            vec3  sunDir   = normalize(pc.sunDir.xyz);
            vec3  sunHoriz = sunDir - dot(sunDir, up) * up;
            float sunHLen  = length(sunHoriz);
            vec3  sunProj  = (sunHLen > 1e-4) ? (sunHoriz / sunHLen) : vec3(1.0, 0.0, 0.0);
            vec3  perpAxis = cross(up, sunProj);

            float phi   = uv.x * PI;
            float theta = skyViewDecodeV(uv.y, h, pc.bottomRadius);

            // phi=0 → toward sun horizontally; matches sampleSkyView convention
            vec3 rd = cos(theta) * (cos(phi) * sunProj + sin(phi) * perpAxis)
                    + sin(theta) * up;

            vec2  gndHit  = raySphereIntersect(viewPos, rd, pc.bottomRadius);
            float maxDist = 1e12;
            if (gndHit.x > 0.0 && gndHit.x < gndHit.y)
                maxDist = gndHit.x;

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