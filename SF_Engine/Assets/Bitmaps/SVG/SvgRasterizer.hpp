#pragma once
#include <cstdint>
#include <vector>

namespace SF::Engine::SvgRaster
{
    struct Point { float x, y; };
    using Contour = std::vector<Point>;    // implicitly closed for fill
    using ContourSet = std::vector<Contour>;

    struct Rgba { uint8_t r, g, b, a; };

    struct PixelBuffer
    {
        int width = 0;
        int height = 0;
        std::vector<Rgba> pixels; // row-major, top-left origin

        void Resize(int w, int h, Rgba clear = {0, 0, 0, 0})
        {
            width = w;
            height = h;
            pixels.assign(static_cast<size_t>(w) * h, clear);
        }
    };

    // Nonzero/even-odd fill with 4x4 supersampled AA, alpha-blended onto target.
    void FillContours(PixelBuffer& target, const ContourSet& contours, Rgba color, bool evenOdd);

    // Distance-field stroke: rasterized as ONE coverage pass so overlapping
    // segments/joins don't double-blend into visible seams.
    void StrokeContours(PixelBuffer& target, const ContourSet& polylines, bool closed,
                         float strokeWidth, Rgba color);
}