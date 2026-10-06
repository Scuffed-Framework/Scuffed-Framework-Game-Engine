// HiZDownsample.shader : one mip of the Hi-Z (max-reduction) depth pyramid.
//
// Reversed-Z (near=1, far->0): max keeps the CLOSEST depth in each 2x2 block, which is what a
// conservative "can this ray pass through this cell?" traversal needs (see Trace.shader).
//
// Dispatched once per pyramid mip, finest to coarsest:
//   level 0 : hizSrc = gbuf_depth (full res),            hizDst = pyramid mip 0 (half res)
//   level k : hizSrc = pyramid mip k-1 (single-mip view), hizDst = pyramid mip k
//
// Odd source dimensions: the last destination texel along that axis also folds in the extra
// source row/column (3 wide instead of 2), otherwise the rightmost/bottom edge of the depth
// buffer would be silently dropped from every coarser level.
//
// Resource layout (set=0):
//   binding 0   Texture2D           hizSrc
//   binding 1   RWTexture2D<float>  hizDst   (R32_SFLOAT)

[[vk::binding(0, 0)]] Texture2D hizSrc;
[[vk::binding(1, 0)]] RWTexture2D<float> hizDst;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint dw, dh;
    hizDst.GetDimensions(dw, dh);
    if (dispatchThreadID.x >= dw || dispatchThreadID.y >= dh)
        return;

    uint sw, sh;
    hizSrc.GetDimensions(sw, sh);

    int2 base = int2(dispatchThreadID.xy) * 2;
    int2 maxCoord = int2(sw, sh) - 1;

    int xEnd = (((sw & 1u) != 0u) && dispatchThreadID.x == dw - 1u) ? 2 : 1;
    int yEnd = (((sh & 1u) != 0u) && dispatchThreadID.y == dh - 1u) ? 2 : 1;

    float closest = 0.0;
    for (int y = 0; y <= yEnd; ++y)
    {
        for (int x = 0; x <= xEnd; ++x)
        {
            int2 c = min(base + int2(x, y), maxCoord);
            closest = max(closest, hizSrc.Load(int3(c, 0)).r);
        }
    }

    hizDst[int2(dispatchThreadID.xy)] = closest;
}
