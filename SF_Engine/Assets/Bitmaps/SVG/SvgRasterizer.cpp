#include "SvgRasterizer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace SF::Engine::SvgRaster
{
    namespace
    {
        constexpr int kSuperSample = 4;

        bool PointInside(const ContourSet& contours, float px, float py, bool evenOdd)
        {
            int winding = 0;
            for (const Contour& c : contours)
            {
                size_t n = c.size();
                if (n < 2) continue;
                for (size_t i = 0; i < n; ++i)
                {
                    const Point& a = c[i];
                    const Point& b = c[(i + 1) % n];
                    if ((a.y <= py) != (b.y <= py))
                    {
                        float t = (py - a.y) / (b.y - a.y);
                        float xCross = a.x + t * (b.x - a.x);
                        if (xCross > px)
                            winding += (b.y > a.y) ? 1 : -1;
                    }
                }
            }
            return evenOdd ? (winding % 2 != 0) : (winding != 0);
        }

        void BoundingBox(const ContourSet& contours, float& minX, float& minY, float& maxX, float& maxY)
        {
            minX = minY = std::numeric_limits<float>::max();
            maxX = maxY = std::numeric_limits<float>::lowest();
            for (const Contour& c : contours)
                for (const Point& p : c)
                {
                    minX = std::min(minX, p.x); minY = std::min(minY, p.y);
                    maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y);
                }
        }

        void Blend(Rgba& dst, Rgba src, float coverage)
        {
            float srcA = (src.a / 255.0f) * coverage;
            if (srcA <= 0.0f) return;
            float dstA = dst.a / 255.0f;
            float outA = srcA + dstA * (1.0f - srcA);
            if (outA <= 0.0001f) { dst = {0, 0, 0, 0}; return; }
            auto mix = [&](uint8_t s, uint8_t d) {
                float sf = s / 255.0f, df = d / 255.0f;
                float outF = (sf * srcA + df * dstA * (1.0f - srcA)) / outA;
                return static_cast<uint8_t>(std::clamp(outF, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            dst.r = mix(src.r, dst.r);
            dst.g = mix(src.g, dst.g);
            dst.b = mix(src.b, dst.b);
            dst.a = static_cast<uint8_t>(std::clamp(outA, 0.0f, 1.0f) * 255.0f + 0.5f);
        }
    }

    void FillContours(PixelBuffer& target, const ContourSet& contours, Rgba color, bool evenOdd)
    {
        if (contours.empty()) return;

        float minX, minY, maxX, maxY;
        BoundingBox(contours, minX, minY, maxX, maxY);

        int x0 = std::max(0, static_cast<int>(std::floor(minX)));
        int y0 = std::max(0, static_cast<int>(std::floor(minY)));
        int x1 = std::min(target.width - 1, static_cast<int>(std::ceil(maxX)));
        int y1 = std::min(target.height - 1, static_cast<int>(std::ceil(maxY)));
        if (x0 > x1 || y0 > y1) return;

        const float step = 1.0f / kSuperSample;
        const float sampleWeight = 1.0f / (kSuperSample * kSuperSample);

        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                float coverage = 0.0f;
                for (int sy = 0; sy < kSuperSample; ++sy)
                {
                    float py = y + (sy + 0.5f) * step;
                    for (int sx = 0; sx < kSuperSample; ++sx)
                    {
                        float px = x + (sx + 0.5f) * step;
                        if (PointInside(contours, px, py, evenOdd))
                            coverage += sampleWeight;
                    }
                }
                if (coverage > 0.0f)
                    Blend(target.pixels[static_cast<size_t>(y) * target.width + x], color, coverage);
            }
        }
    }

    void StrokeContours(PixelBuffer& target, const ContourSet& polylines, bool closed,
                         float strokeWidth, Rgba color)
    {
        float half = strokeWidth * 0.5f;
        if (half <= 0.0f) return;

        float minX = std::numeric_limits<float>::max(), minY = minX;
        float maxX = std::numeric_limits<float>::lowest(), maxY = maxX;
        for (const Contour& line : polylines)
            for (const Point& p : line)
            {
                minX = std::min(minX, p.x - half); minY = std::min(minY, p.y - half);
                maxX = std::max(maxX, p.x + half); maxY = std::max(maxY, p.y + half);
            }

        int x0 = std::max(0, static_cast<int>(std::floor(minX)));
        int y0 = std::max(0, static_cast<int>(std::floor(minY)));
        int x1 = std::min(target.width - 1, static_cast<int>(std::ceil(maxX)));
        int y1 = std::min(target.height - 1, static_cast<int>(std::ceil(maxY)));
        if (x0 > x1 || y0 > y1) return;

        auto distToSegment = [](float px, float py, Point a, Point b) {
            float dx = b.x - a.x, dy = b.y - a.y;
            float len2 = dx * dx + dy * dy;
            float t = len2 > 0.0f ? std::clamp(((px - a.x) * dx + (py - a.y) * dy) / len2, 0.0f, 1.0f) : 0.0f;
            float cx = a.x + t * dx, cy = a.y + t * dy;
            return std::hypot(px - cx, py - cy);
        };

        const float step = 1.0f / kSuperSample;
        const float sampleWeight = 1.0f / (kSuperSample * kSuperSample);

        for (int y = y0; y <= y1; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                float coverage = 0.0f;
                for (int sy = 0; sy < kSuperSample; ++sy)
                {
                    float py = y + (sy + 0.5f) * step;
                    for (int sx = 0; sx < kSuperSample; ++sx)
                    {
                        float px = x + (sx + 0.5f) * step;
                        bool inside = false;
                        for (const Contour& line : polylines)
                        {
                            size_t n = line.size();
                            size_t segCount = closed ? n : (n > 0 ? n - 1 : 0);
                            for (size_t i = 0; i < segCount; ++i)
                            {
                                if (distToSegment(px, py, line[i], line[(i + 1) % n]) <= half)
                                { inside = true; break; }
                            }
                            if (inside) break;
                        }
                        if (inside) coverage += sampleWeight;
                    }
                }
                if (coverage > 0.0f)
                    Blend(target.pixels[static_cast<size_t>(y) * target.width + x], color, coverage);
            }
        }
    }
}