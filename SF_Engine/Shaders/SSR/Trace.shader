// Trace.shader — Probed Stochastic SSR, stage 2/5.
//
// Consumes the per-pixel stochastic direction from RayGen.shader and traces it with a Hi-Z
// (hierarchical max-depth) traversal in screen space — a port of the AMD FidelityFX SSSR scheme
// (advanceRay / hizMarching / validateHit), adapted to this engine's reversed-Z depth buffer,
// world-space octahedral normals and the Hi-Z pyramid built by HiZPipelinePass.
//
// Why this replaced the old linear world-space march: that march accepted any step that landed
// within a fixed thickness *behind* a surface, so every floor ray that passed behind a
// silhouette registered as a hit (the "cube reflection stretched into a wedge" artifact), and
// its stride was unrelated to pixel footprint, so hit/miss was essentially random per pixel.
// The Hi-Z traversal finds the true first crossing, and ValidateHit() then rejects back-face
// hits and fades confidence with the distance between the ray and the surface it "hit".
//
// Hi-Z level indexing: level 0 = gbuf_depth, level L>=1 = hizPyramid mip (L-1);
// resolution of level L = max(1, screenSize >> L). See HiZPipelinePass.hpp.
//
// The reflected colour is read from "hdr", which at this point is LAST frame's resolved image
// (see SSRPipelinePass::PreRender), so the hit is reprojected with prevViewProjection (correct
// for static geometry; moving objects will smear by one frame of motion).
//
// Resource layout (Vulkan target, single descriptor set, set=0):
//   binding 0   ConstantBuffer SSRParams
//   binding 1   Texture2D      gbufDepth
//   binding 2   Texture2D      gbufNormal
//   binding 3   Texture2D      hdrScene        (previous frame's lit scene colour)
//   binding 4   Texture2D      inRayDir        (from RayGen, rgb=dir, a=NdotH)
//   binding 5   Texture2D      inRayData       (from RayGen, r=roughnessA g=metallic b=skyMask a=pdf)
//   binding 6   RWTexture2D<float4> imgTraceColor (rgb=radiance, a=confidence)
//   binding 7   RWTexture2D<float4> imgTraceHit   (r=hitMask, g=hitWorldDist, b=pdf, a=unused)
//   binding 10  Texture2D      hizPyramid      (R32F max-depth pyramid, GENERAL layout)
//   binding 11  TextureCube    probeCube       (prefiltered environment probe, GENERAL layout)
//   binding 31  ConstantBuffer Camera (shared)
//
// BINDING NUMBERS 0-9 vs THE SHARED SAMPLERS: Shader::ReflectFromSPIRV flattens the descriptor
// bindings of EVERY set into one set-0 layout, de-duplicated by binding number. The shared samplers
// (Common/Samplers.si, set 1) occupy bindings 0-9, so a sampler this shader actually uses can
// clobber a set-0 resource that has the same number (that is what broke hizPyramid at 8 once the
// mip-filter sampler, also 8, was used). New set-0 resources here therefore start at 10.
//
// SSRParams usage: maxSteps = max Hi-Z traversal iterations, thickness = world-space distance
// over which hit confidence falls to zero. strideScale / binarySearchSteps /
// depthBufferThicknessBias are no longer used by this stage.
#include "SSR/SSRCommon.si"
#include "Common/Samplers.si"
#include "ReflectionProbe/ReflectionProbe.si"

[[vk::binding(0, 0)]] ConstantBuffer<SSRParams> kSSR;
[[vk::binding(1, 0)]] Texture2D gbufDepth;
[[vk::binding(2, 0)]] Texture2D gbufNormal;
[[vk::binding(3, 0)]] Texture2D hdrScene;
[[vk::binding(4, 0)]] Texture2D inRayDir;
[[vk::binding(5, 0)]] Texture2D inRayData;
[[vk::binding(6, 0)]] RWTexture2D<float4> imgTraceColor;
[[vk::binding(7, 0)]] RWTexture2D<float4> imgTraceHit;
[[vk::binding(10, 0)]] Texture2D hizPyramid;
[[vk::binding(11, 0)]] TextureCube probeCube;
[[vk::binding(SSR_CAMERA_BIND, 0)]] ConstantBuffer<Camera> kCam;

static const float kFltMax = 3.402823466e+38;

// Environment lookup into the reflection probe cube (see ReflectionProbePipelinePass): what a ray
// sees when the screen-space trace has nothing to say -- a miss, a ray that left the screen, or a
// surface too rough to trace. `mip` selects the prefiltered roughness level (0 = raw radiance).
float3 ProbeFallback(float3 dir, float mip)
{
    float3 c = probeCube.SampleLevel(linearClampEdgeMipFilterSampler, dir, mip).rgb;
    if (anyBadFloat(c))
        return float3(0.0, 0.0, 0.0);
    return c;
}

// ---------------------------------------------------------------------
// Hi-Z access
// ---------------------------------------------------------------------

int2 HizLevelRes(int level)
{
    return max(int2(1, 1), int2(kSSR.screenSize) >> level);
}

// Closest depth in the given cell. Out-of-range reads return 0 (= background in reversed-Z).
float HizLoad(int2 coord, int level, int2 res)
{
    if (any(coord < int2(0, 0)) || any(coord >= res))
        return 0.0;
    if (level == 0)
        return gbufDepth.Load(int3(coord, 0)).r;
    return hizPyramid.Load(int3(coord, level - 1)).r;
}

// Advances the ray to the next cell boundary of the current Hi-Z level, or clamps it to the
// surface plane if the surface lies inside the cell. Returns true if the whole cell was skipped
// (so the caller should climb to a coarser level), false if it must refine to a finer one.
bool AdvanceRay(float3 origin, float3 direction, float3 invDirection,
                float2 currentMipPosition, float2 currentMipResInv,
                float2 floorOffset, float2 uvOffset, float surfaceZ,
                inout float3 position, inout float currentT)
{
    float2 xyPlane = floor(currentMipPosition) + floorOffset;
    xyPlane = xyPlane * currentMipResInv + uvOffset;
    float3 boundaryPlanes = float3(xyPlane, surfaceZ);

    // o + d * t = p'  =>  t = (p' - o) / d
    float3 t = boundaryPlanes * invDirection - origin * invDirection;

    // Reversed-Z: only a ray moving AWAY from the camera (z decreasing) can cross the surface
    // plane; otherwise ignore the z plane so the ray can't stall on it.
    t.z = direction.z < 0.0 ? t.z : kFltMax;

    float tMin = min(min(t.x, t.y), t.z);

    // Reversed-Z: surface is farther than the ray  <=>  surfaceZ < position.z
    bool bAboveSurface = surfaceZ < position.z;

    // Bitwise compare on purpose (avoids NaN/Inf logic): did the nearest boundary come from the xy planes?
    bool bSkipTile = (asuint(tMin) != asuint(t.z)) && bAboveSurface;

    // Only advance while above the surface; otherwise hold position and let the caller refine.
    currentT = bAboveSurface ? tMin : currentT;
    position = origin + currentT * direction;

    return bSkipTile;
}

// Hi-Z traversal in (uv.xy, ndcDepth) space. origin/dir are in that space; dir is NOT normalised
// (it is end - start of the projected segment). tMax bounds the parametric range (1.0 when the
// segment was clipped against the near plane, huge otherwise).
// Returns true if the ray reached the finest level and stopped on/below a surface.
bool HizMarch(float3 origin, float3 dir, float tMax, int maxLevel, int maxIterations, out float3 hit)
{
    hit = origin;

    float3 invDir = float3(dir.x != 0.0 ? 1.0 / dir.x : kFltMax,
                           dir.y != 0.0 ? 1.0 / dir.y : kFltMax,
                           dir.z != 0.0 ? 1.0 / dir.z : kFltMax);

    float2 sgn = float2(dir.x > 0.0 ? 1.0 : (dir.x < 0.0 ? -1.0 : 0.0),
                        dir.y > 0.0 ? 1.0 : (dir.y < 0.0 ? -1.0 : 0.0));

    int level = 0;
    int2 res = HizLevelRes(0);
    float2 resInv = 1.0 / float2(res);

    // Nudge into the neighbouring cell so the ray doesn't re-hit the cell it starts in.
    float2 uvOffset = 0.005 * kSSR.invScreenSize * sgn;

    // Selects the cell border the ray is heading towards.
    float2 floorOffset = float2(dir.x < 0.0 ? 0.0 : 1.0, dir.y < 0.0 ? 0.0 : 1.0);

    float currentT;
    float3 position;
    {
        // Initial advance out of the starting cell (self-hit avoidance).
        float2 currentMipPosition = float2(res) * origin.xy;
        float2 xyPlane = (floor(currentMipPosition) + floorOffset) * resInv + uvOffset;
        float2 t = xyPlane * invDir.xy - origin.xy * invDir.xy;
        currentT = min(t.x, t.y);
        position = origin + currentT * dir;
    }

    int i = 0;
    while (i < maxIterations && level >= 0)
    {
        // Left the screen, or ran past the near-plane-clipped end of the segment: no hit.
        if (currentT > tMax || any(position.xy < float2(0.0, 0.0)) || any(position.xy >= float2(1.0, 1.0)))
            return false;

        float2 currentMipPosition = float2(res) * position.xy;
        float surfaceZ = HizLoad(int2(currentMipPosition), level, res);

        bool bSkipTile = AdvanceRay(origin, dir, invDir, currentMipPosition, resInv,
                                    floorOffset, uvOffset, surfaceZ, position, currentT);

        level = bSkipTile ? min(level + 1, maxLevel) : level - 1;
        res = HizLevelRes(max(level, 0));
        resInv = 1.0 / float2(res);
        ++i;
    }

    hit = position;
    // Only a ray that refined all the way below level 0 found a surface; running out of
    // iterations is a miss, not a hit.
    return level < 0;
}

// Accepts or rejects a Hi-Z hit and returns its confidence in [0,1]. Fills surfaceWS with the
// world position of the surface the ray stopped on (used to reproject the colour lookup).
float ValidateHit(float3 hit, float2 uv, float3 rayDirWS, out float3 surfaceWS)
{
    surfaceWS = float3(0.0, 0.0, 0.0);

    if (any(hit.xy < float2(0.0, 0.0)) || any(hit.xy >= float2(1.0, 1.0)))
        return 0.0;

    // Reject hits that barely left the origin pixel (immediate self reflection).
    float2 manhattan = abs(hit.xy - uv);
    float2 manhattanEdge = 2.0 * kSSR.invScreenSize;
    if (manhattan.x < manhattanEdge.x && manhattan.y < manhattanEdge.y)
        return 0.0;

    int2 px = int2(hit.xy * kSSR.screenSize);
    float surfaceZ = gbufDepth.Load(int3(px, 0)).r;
    if (surfaceZ <= 0.0) // background: nothing to reflect
        return 0.0;

    // Back-face hit (ray hit the surface from behind, e.g. passed behind a silhouette): reject.
    float3 hitN = SSR_OctDecodeNormal(gbufNormal.Load(int3(px, 0)).rg);
    if (dot(hitN, rayDirWS) > 0.0)
        return 0.0;

    surfaceWS = SSR_WorldPosFromDepth(hit.xy, surfaceZ, kCam.inverseProjection, kCam.inverseView);
    float3 hitWS = SSR_WorldPosFromDepth(hit.xy, hit.z, kCam.inverseProjection, kCam.inverseView);
    float dist = length(surfaceWS - hitWS);

    // Fade hits near the screen border.
    float2 fov = 0.05 * float2(kSSR.screenSize.y / kSSR.screenSize.x, 1.0);
    float2 border = smoothstep(float2(0.0, 0.0), fov, hit.xy) *
                    (1.0 - smoothstep(float2(1.0, 1.0) - fov, float2(1.0, 1.0), hit.xy));
    float vignette = border.x * border.y;

    // Constant-in-world-space falloff: hits that end up far *behind* the surface they landed on
    // (thin occluders) fade out instead of growing the reflection toward the reflected object.
    float confidence = 1.0 - smoothstep(0.0, max(kSSR.thickness, 1e-4), dist);
    confidence *= confidence;

    return vignette * confidence;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    int2 workPos = int2(dispatchThreadID.xy);
    int2 texSize = int2(kSSR.screenSize);
    if (workPos.x >= texSize.x || workPos.y >= texSize.y)
        return;

    float4 rayDir4 = inRayDir.Load(int3(workPos, 0));
    float4 rayData = inRayData.Load(int3(workPos, 0));
    bool bSky = rayData.b > 0.9;       // true background -- RayGen.shader's depth<=0 case
    bool bTooRough = rayData.b > 0.4 && rayData.b < 0.6; // RayGen's roughness-cutoff case (b=0.5)

    float2 uv = (float2(workPos) + 0.5) * kSSR.invScreenSize;
    float depth = gbufDepth.Load(int3(workPos, 0)).r;

    if (bSky || depth <= 0.0 || dot(rayDir4.xyz, rayDir4.xyz) < 1e-6)
    {
        imgTraceColor[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        imgTraceHit[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float pdf = rayData.a;

    // Too-rough pixel: RayGen skipped the lobe sample and put the surface NORMAL in rayDir4.xyz (and
    // the perceptual roughness in rayData.r). No trace: look up the mirror direction in the
    // prefiltered probe at that roughness. The prefilter already integrates the whole lobe, so this
    // is smooth and noise-free -- and it replaces the entire march for these pixels.
    if (bTooRough)
    {
        if (kSSR.bProbeFallbackEnabled != 0)
        {
            float3 N = normalize(rayDir4.xyz);
            float3 wp = SSR_WorldPosFromDepth(uv, depth, kCam.inverseProjection, kCam.inverseView);
            float3 V = normalize(kCam.cameraPosition.xyz - wp);
            float3 R = reflect(-V, N);
            float3 probeColor = ProbeFallback(R, ProbeRoughnessToMip(rayData.r));
            imgTraceColor[workPos] = float4(probeColor, 1.0);
        }
        else
        {
            imgTraceColor[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        }
        imgTraceHit[workPos] = float4(0.0, 0.0, pdf, 0.0);
        return;
    }

    float3 worldPos = SSR_WorldPosFromDepth(uv, depth, kCam.inverseProjection, kCam.inverseView);
    float3 rayDirWorld = normalize(rayDir4.xyz);

    // --- Build the screen-space ray: start = (uv, depth), end = projected view-space segment. ---
    float3 viewP = mul(kCam.view, float4(worldPos, 1.0)).xyz;
    float3 viewD = mul(kCam.view, float4(rayDirWorld, 0.0)).xyz;

    // View space looks down -Z; forward distance = -z.
    float fz0 = -viewP.z;
    float fd = -viewD.z;

    float segLen = max(fz0, 1.0);
    float tMax = kFltMax;
    const float kNearEps = 0.05;
    if (fd < 0.0 && fz0 + fd * segLen < kNearEps)
    {
        // Ray heads back toward the camera: clip the segment at the near plane (projecting a point
        // behind the camera would flip the screen-space direction) and don't march past it.
        segLen = (kNearEps - fz0) / fd;
        tMax = 1.0;
    }

    float3 viewEnd = viewP + viewD * segLen;
    float4 clipEnd = mul(kCam.projection, float4(viewEnd, 1.0));
    float3 ndcEnd = clipEnd.xyz / max(clipEnd.w, 1e-6);

    float3 ssStart = float3(uv, depth);
    // Projection is already Y-flipped (see SSRCommon.si), so ndc.y maps straight to screen-down uv.y.
    float3 ssEnd = float3(ndcEnd.x * 0.5 + 0.5, ndcEnd.y * 0.5 + 0.5, ndcEnd.z);
    float3 ssDir = ssEnd - ssStart;

    bool bHit = false;
    float confidence = 0.0;
    float hitDist = 0.0;
    float3 hitColor = float3(0.0, 0.0, 0.0);

    // Ray pointing (almost) straight at / away from the camera has no screen-space extent.
    if (dot(ssDir.xy, ssDir.xy) > 1e-12)
    {
        uint hizW, hizH, hizLevels;
        hizPyramid.GetDimensions(0, hizW, hizH, hizLevels);

        float3 hit;
        bool bMarched = HizMarch(ssStart, ssDir, tMax, int(hizLevels), kSSR.maxSteps, hit);
        if (bMarched)
        {
            float3 surfaceWS;
            confidence = ValidateHit(hit, uv, rayDirWorld, surfaceWS);

            if (confidence > 0.0)
            {
                // "hdr" is last frame's image: look the hit up where it was LAST frame.
                float3 prevUVZ = SSR_WorldToScreenUV(surfaceWS, kCam.prevViewProjection);
                if (SSR_UvInBounds(prevUVZ.xy))
                {
                    hitColor = hdrScene.SampleLevel(linearClampEdgeSampler, prevUVZ.xy, 0).rgb;
                    if (anyBadFloat(hitColor))
                        hitColor = float3(0.0, 0.0, 0.0);

                    float3 hitWS = SSR_WorldPosFromDepth(hit.xy, hit.z, kCam.inverseProjection, kCam.inverseView);
                    hitDist = length(hitWS - worldPos);
                    bHit = true;
                }
                else
                {
                    confidence = 0.0;
                }
            }
        }
    }

    // The direction was importance-sampled from the specular lobe (and is accumulated over frames by
    // the temporal pass), so read the RAW cube (mip 0): sampling a roughness-prefiltered mip as well
    // would blur the lobe twice.
    float3 probeColor = float3(0.0, 0.0, 0.0);
    if (kSSR.bProbeFallbackEnabled != 0)
        probeColor = ProbeFallback(rayDirWorld, 0.0);

    if (bHit)
    {
        // Partial confidence blends toward the probe instead of fading to black.
        if (kSSR.bProbeFallbackEnabled != 0)
        {
            imgTraceColor[workPos] = float4(lerp(probeColor, hitColor, confidence), 1.0);
        }
        else
        {
            imgTraceColor[workPos] = float4(hitColor * confidence, confidence);
        }
        imgTraceHit[workPos] = float4(1.0, hitDist, pdf, 0.0);
    }
    else if (kSSR.bProbeFallbackEnabled != 0)
    {
        imgTraceColor[workPos] = float4(probeColor, 1.0);
        imgTraceHit[workPos] = float4(0.0, 0.0, pdf, 0.0);
    }
    else
    {
        imgTraceColor[workPos] = float4(0.0, 0.0, 0.0, 0.0);
        imgTraceHit[workPos] = float4(0.0, 0.0, pdf, 0.0);
    }
}
