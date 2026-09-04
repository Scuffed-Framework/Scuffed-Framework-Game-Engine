#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <unordered_map>

#include "SvgRasterizer.hpp"
#include "SvgXml.hpp"

#include <Assets/Bitmaps/Bitmap.hpp>

namespace SF::Engine
{
    class SvgColor
    {
    public:
        SvgColor() = default;

        static SvgColor None();
        static SvgColor Named(std::string name);
        static SvgColor Rgb(uint8_t r, uint8_t g, uint8_t b);
        static SvgColor Rgba(uint8_t r, uint8_t g, uint8_t b, float a);

        const std::string &ToString() const
        {
            return m_Value;
        }

    private:
        std::string m_Value = "#000000";
    };

    struct SvgStyle
    {
        std::optional<SvgColor> fill;
        std::optional<SvgColor> stroke;
        std::optional<float> strokeWidth;
        std::optional<float> opacity;
        std::optional<float> fillOpacity;
        std::optional<float> strokeOpacity;
        std::string strokeLinecap;  // "butt" | "round" | "square"
        std::string strokeLinejoin; // "miter" | "round" | "bevel"
        std::string transform;
        std::string id;
        std::string cssClass;

        SvgStyle &WithFill(SvgColor color)
        {
            fill = std::move(color);
            return *this;
        }
        SvgStyle &WithStroke(SvgColor color, float width = 1.0f)
        {
            stroke = std::move(color);
            strokeWidth = width;
            return *this;
        }
        SvgStyle &WithOpacity(float value)
        {
            opacity = value;
            return *this;
        }
        SvgStyle &WithTransform(std::string value)
        {
            transform = std::move(value);
            return *this;
        }
        SvgStyle &WithId(std::string value)
        {
            id = std::move(value);
            return *this;
        }
        SvgStyle &WithClass(std::string value)
        {
            cssClass = std::move(value);
            return *this;
        }

        void WriteAttributes(std::ostream &os) const;
    };

    struct SvgRect
    {
        Vec2 position{};
        Vec2 size{};
        float rx = 0.0f;
        float ry = 0.0f;
        SvgStyle style;
    };

    struct SvgCircle
    {
        Vec2 center{};
        float radius = 0.0f;
        SvgStyle style;
    };

    struct SvgEllipse
    {
        Vec2 center{};
        Vec2 radius{};
        SvgStyle style;
    };

    struct SvgLine
    {
        Vec2 start{};
        Vec2 end{};
        SvgStyle style;
    };

    struct SvgPolyline
    {
        std::vector<Vec2> points;
        SvgStyle style;
    };

    struct SvgPolygon
    {
        std::vector<Vec2> points;
        SvgStyle style;
    };

    struct SvgPath
    {
        std::string data; // the 'd' attribute; build with SvgPathBuilder
        SvgStyle style;
    };

    struct SvgText
    {
        Vec2 position{};
        std::string content;
        float fontSize = 16.0f;
        std::string fontFamily = "sans-serif";
        SvgStyle style;
    };

    struct SvgGroup;

    using SvgElement = std::variant<
        SvgRect, SvgCircle, SvgEllipse, SvgLine,
        SvgPolyline, SvgPolygon, SvgPath, SvgText,
        std::unique_ptr<SvgGroup>>;

    struct SvgGroup
    {
        SvgStyle style;
        std::vector<SvgElement> elements;
    };

    class SvgDocument
    {
    public:
        void Write(const std::filesystem::path &filename) const;

        float width = 100.0f;
        float height = 100.0f;
        std::optional<std::array<float, 4>> viewBox; // minX, minY, width, height
        std::string title;
        std::vector<SvgElement> elements;
    };

    class BitmapSvg : public Bitmap::Registrar<BitmapSvg>
    {
    public:
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename);
        static void Write(const Bitmap &bitmap, const std::filesystem::path &filename);
        // todo: impl & take data from svgbuilder

    private:
        static inline bool registered = Register("svg", "SVG");
    };
    
    namespace
    {
        // ---- affine transform (row: [a c e; b d f]) ----
        struct Mat2x3
        {
            float a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
        };

        Mat2x3 Multiply(const Mat2x3 &m1, const Mat2x3 &m2) // m1 applied after m2
        {
            return {
                m1.a * m2.a + m1.c * m2.b, m1.b * m2.a + m1.d * m2.b,
                m1.a * m2.c + m1.c * m2.d, m1.b * m2.c + m1.d * m2.d,
                m1.a * m2.e + m1.c * m2.f + m1.e, m1.b * m2.e + m1.d * m2.f + m1.f};
        }

        SvgRaster::Point Apply(const Mat2x3 &m, float x, float y)
        {
            return {m.a * x + m.c * y + m.e, m.b * x + m.d * y + m.f};
        }

        std::vector<float> ParseNumberList(const std::string &s)
        {
            std::vector<float> out;
            size_t i = 0;
            while (i < s.size())
            {
                while (i < s.size() && (std::isspace((unsigned char)s[i]) || s[i] == ','))
                    ++i;
                size_t start = i;
                if (i < s.size() && (s[i] == '+' || s[i] == '-'))
                    ++i;
                bool saw = false;
                while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.'))
                {
                    saw = true;
                    ++i;
                }
                if (i < s.size() && (s[i] == 'e' || s[i] == 'E'))
                {
                    ++i;
                    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
                        ++i;
                    while (i < s.size() && std::isdigit((unsigned char)s[i]))
                        ++i;
                }
                if (saw && start < i)
                    out.push_back(std::strtof(s.substr(start, i - start).c_str(), nullptr));
                else if (i == start)
                    ++i;
            }
            return out;
        }

        Mat2x3 ParseTransform(const std::string &value)
        {
            Mat2x3 result;
            size_t i = 0;
            while (i < value.size())
            {
                while (i < value.size() && (std::isspace((unsigned char)value[i]) || value[i] == ','))
                    ++i;
                size_t nameStart = i;
                while (i < value.size() && std::isalpha((unsigned char)value[i]))
                    ++i;
                std::string name = value.substr(nameStart, i - nameStart);
                if (name.empty())
                    break;
                size_t open = value.find('(', i);
                size_t close = value.find(')', open == std::string::npos ? i : open);
                if (open == std::string::npos || close == std::string::npos)
                    break;
                auto args = ParseNumberList(value.substr(open + 1, close - open - 1));
                Mat2x3 m;
                if (name == "translate")
                {
                    m.e = args.size() > 0 ? args[0] : 0;
                    m.f = args.size() > 1 ? args[1] : 0;
                }
                else if (name == "scale")
                {
                    m.a = args.size() > 0 ? args[0] : 1;
                    m.d = args.size() > 1 ? args[1] : m.a;
                }
                else if (name == "rotate")
                {
                    float rad = (args.empty() ? 0 : args[0]) * 3.14159265358979323846f / 180.0f;
                    float cs = std::cos(rad), sn = std::sin(rad);
                    if (args.size() >= 3)
                    {
                        Mat2x3 t1{1, 0, 0, 1, args[1], args[2]};
                        Mat2x3 r{cs, sn, -sn, cs, 0, 0};
                        Mat2x3 t2{1, 0, 0, 1, -args[1], -args[2]};
                        m = Multiply(t1, Multiply(r, t2));
                    }
                    else
                        m = {cs, sn, -sn, cs, 0, 0};
                }
                else if (name == "skewX")
                {
                    float rad = (args.empty() ? 0 : args[0]) * 3.14159265358979323846f / 180.0f;
                    m = {1, 0, std::tan(rad), 1, 0, 0};
                }
                else if (name == "skewY")
                {
                    float rad = (args.empty() ? 0 : args[0]) * 3.14159265358979323846f / 180.0f;
                    m = {1, std::tan(rad), 0, 1, 0, 0};
                }
                else if (name == "matrix" && args.size() >= 6)
                    m = {args[0], args[1], args[2], args[3], args[4], args[5]};
                result = Multiply(result, m);
                i = close + 1;
            }
            return result;
        }

        const std::unordered_map<std::string, uint32_t> &NamedColors()
        {
            static const std::unordered_map<std::string, uint32_t> table = {
                {"black", 0x000000},
                {"white", 0xffffff},
                {"red", 0xff0000},
                {"green", 0x008000},
                {"blue", 0x0000ff},
                {"yellow", 0xffff00},
                {"cyan", 0x00ffff},
                {"magenta", 0xff00ff},
                {"gray", 0x808080},
                {"grey", 0x808080},
                {"orange", 0xffa500},
                {"purple", 0x800080},
                {"pink", 0xffc0cb},
                {"brown", 0xa52a2a},
                {"lime", 0x00ff00},
                {"navy", 0x000080},
                {"teal", 0x008080},
                {"silver", 0xc0c0c0},
                {"maroon", 0x800000},
                {"olive", 0x808000},
            };
            return table;
        }

        struct ParsedColor
        {
            SvgRaster::Rgba rgba{};
            bool isNone = false;
            bool valid = true;
        };

        ParsedColor ParseColor(std::string value, float opacity)
        {
            auto trim = [](std::string s)
            {
                size_t b = s.find_first_not_of(" \t\r\n");
                size_t e = s.find_last_not_of(" \t\r\n");
                return (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);
            };
            value = trim(value);
            ParsedColor result;
            if (value.empty() || value == "none" || value == "transparent")
            {
                result.isNone = true;
                return result;
            }

            uint8_t r = 0, g = 0, b = 0;
            float a = 1.0f;
            if (value[0] == '#')
            {
                std::string hex = value.substr(1);
                if (hex.size() == 3)
                {
                    r = static_cast<uint8_t>(std::stoi(std::string(2, hex[0]), nullptr, 16));
                    g = static_cast<uint8_t>(std::stoi(std::string(2, hex[1]), nullptr, 16));
                    b = static_cast<uint8_t>(std::stoi(std::string(2, hex[2]), nullptr, 16));
                }
                else if (hex.size() >= 6)
                {
                    r = static_cast<uint8_t>(std::stoi(hex.substr(0, 2), nullptr, 16));
                    g = static_cast<uint8_t>(std::stoi(hex.substr(2, 2), nullptr, 16));
                    b = static_cast<uint8_t>(std::stoi(hex.substr(4, 2), nullptr, 16));
                }
                else
                    result.valid = false;
            }
            else if (value.rfind("rgb", 0) == 0)
            {
                size_t open = value.find('('), close = value.find(')');
                if (open != std::string::npos && close != std::string::npos)
                {
                    auto parts = ParseNumberList(value.substr(open + 1, close - open - 1));
                    if (parts.size() >= 3)
                    {
                        r = static_cast<uint8_t>(std::clamp(parts[0], 0.0f, 255.0f));
                        g = static_cast<uint8_t>(std::clamp(parts[1], 0.0f, 255.0f));
                        b = static_cast<uint8_t>(std::clamp(parts[2], 0.0f, 255.0f));
                        if (parts.size() >= 4)
                            a = std::clamp(parts[3], 0.0f, 1.0f);
                    }
                }
            }
            else
            {
                auto it = NamedColors().find(value);
                if (it != NamedColors().end())
                {
                    r = static_cast<uint8_t>((it->second >> 16) & 0xff);
                    g = static_cast<uint8_t>((it->second >> 8) & 0xff);
                    b = static_cast<uint8_t>(it->second & 0xff);
                }
                else
                    result.valid = false;
            }
            result.rgba = {r, g, b, static_cast<uint8_t>(std::clamp(a * opacity, 0.0f, 1.0f) * 255.0f)};
            return result;
        }

        struct Style
        {
            std::optional<SvgRaster::Rgba> fill = SvgRaster::Rgba{0, 0, 0, 255};
            std::optional<SvgRaster::Rgba> stroke;
            float strokeWidth = 1.0f;
            float fillOpacity = 1.0f;
            float strokeOpacity = 1.0f;
            float opacity = 1.0f;
            bool evenOdd = false;
        };

        std::string LocalName(const std::string &tag)
        {
            size_t colon = tag.find(':');
            return colon == std::string::npos ? tag : tag.substr(colon + 1);
        }

        std::optional<std::string> StyleLookup(const SvgXml::Node &node, const std::string &attr)
        {
            if (const std::string *v = node.Attr(attr))
                return *v;
            if (const std::string *style = node.Attr("style"))
            {
                std::string s = *style, needle = attr + ":";
                size_t pos = s.find(needle);
                if (pos != std::string::npos)
                {
                    size_t start = pos + needle.size();
                    size_t end = s.find(';', start);
                    std::string v = s.substr(start, end == std::string::npos ? std::string::npos : end - start);
                    size_t b = v.find_first_not_of(" \t"), e = v.find_last_not_of(" \t");
                    if (b != std::string::npos)
                        return v.substr(b, e - b + 1);
                }
            }
            return std::nullopt;
        }

        Style ResolveStyle(const SvgXml::Node &node, const Style &inherited)
        {
            Style s = inherited;
            s.opacity = 1.0f; // element opacity is applied per-element below, not inherited multiplicatively

            if (auto v = StyleLookup(node, "fill-opacity"))
                s.fillOpacity = std::strtof(v->c_str(), nullptr);
            if (auto v = StyleLookup(node, "stroke-opacity"))
                s.strokeOpacity = std::strtof(v->c_str(), nullptr);
            if (auto v = StyleLookup(node, "opacity"))
                s.opacity = std::strtof(v->c_str(), nullptr);
            if (auto v = StyleLookup(node, "stroke-width"))
                s.strokeWidth = std::strtof(v->c_str(), nullptr);
            if (auto v = StyleLookup(node, "fill-rule"))
                s.evenOdd = (*v == "evenodd");

            if (auto v = StyleLookup(node, "fill"))
            {
                ParsedColor pc = ParseColor(*v, s.fillOpacity);
                if (pc.isNone)
                    s.fill = std::nullopt;
                else if (pc.valid)
                    s.fill = pc.rgba;
            }
            if (auto v = StyleLookup(node, "stroke"))
            {
                ParsedColor pc = ParseColor(*v, s.strokeOpacity);
                if (pc.isNone)
                    s.stroke = std::nullopt;
                else if (pc.valid)
                    s.stroke = pc.rgba;
            }
            return s;
        }

        void FlattenCubic(SvgRaster::Contour &out, SvgRaster::Point p0, SvgRaster::Point p1,
                          SvgRaster::Point p2, SvgRaster::Point p3, int segments = 16)
        {
            for (int i = 1; i <= segments; ++i)
            {
                float t = static_cast<float>(i) / segments, mt = 1 - t;
                out.push_back({mt * mt * mt * p0.x + 3 * mt * mt * t * p1.x + 3 * mt * t * t * p2.x + t * t * t * p3.x,
                               mt * mt * mt * p0.y + 3 * mt * mt * t * p1.y + 3 * mt * t * t * p2.y + t * t * t * p3.y});
            }
        }

        void FlattenQuad(SvgRaster::Contour &out, SvgRaster::Point p0, SvgRaster::Point p1,
                         SvgRaster::Point p2, int segments = 16)
        {
            for (int i = 1; i <= segments; ++i)
            {
                float t = static_cast<float>(i) / segments, mt = 1 - t;
                out.push_back({mt * mt * p0.x + 2 * mt * t * p1.x + t * t * p2.x, mt * mt * p0.y + 2 * mt * t * p1.y + t * t * p2.y});
            }
        }

        // Standard endpoint-to-center elliptical arc -> polyline approximation.
        void FlattenArc(SvgRaster::Contour &out, SvgRaster::Point from, float rx, float ry,
                        float xRotDeg, bool largeArc, bool sweep, SvgRaster::Point to)
        {
            if (rx == 0.0f || ry == 0.0f || (from.x == to.x && from.y == to.y))
            {
                out.push_back(to);
                return;
            }
            float phi = xRotDeg * 3.14159265358979323846f / 180.0f;
            float cosPhi = std::cos(phi), sinPhi = std::sin(phi);
            float dx2 = (from.x - to.x) / 2.0f, dy2 = (from.y - to.y) / 2.0f;
            float x1p = cosPhi * dx2 + sinPhi * dy2, y1p = -sinPhi * dx2 + cosPhi * dy2;

            rx = std::abs(rx);
            ry = std::abs(ry);
            float lambda = (x1p * x1p) / (rx * rx) + (y1p * y1p) / (ry * ry);
            if (lambda > 1.0f)
            {
                float sc = std::sqrt(lambda);
                rx *= sc;
                ry *= sc;
            }

            float sign = (largeArc != sweep) ? 1.0f : -1.0f;
            float num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
            float den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
            float coef = den > 0.0f ? sign * std::sqrt(std::max(0.0f, num / den)) : 0.0f;
            float cxp = coef * (rx * y1p / ry), cyp = coef * -(ry * x1p / rx);
            float cx = cosPhi * cxp - sinPhi * cyp + (from.x + to.x) / 2.0f;
            float cy = sinPhi * cxp + cosPhi * cyp + (from.y + to.y) / 2.0f;

            auto angle = [](float ux, float uy, float vx, float vy)
            {
                float dot = ux * vx + uy * vy, len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
                float a = std::acos(std::clamp(dot / len, -1.0f, 1.0f));
                return (ux * vy - uy * vx < 0.0f) ? -a : a;
            };
            float theta1 = angle(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
            float dtheta = angle((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);
            if (!sweep && dtheta > 0)
                dtheta -= 2 * 3.14159265358979323846f;
            if (sweep && dtheta < 0)
                dtheta += 2 * 3.14159265358979323846f;

            int segments = std::max(2, static_cast<int>(std::ceil(std::abs(dtheta) / (3.14159265358979323846f / 16.0f))));
            for (int i = 1; i <= segments; ++i)
            {
                float t = theta1 + dtheta * (static_cast<float>(i) / segments);
                out.push_back({cosPhi * rx * std::cos(t) - sinPhi * ry * std::sin(t) + cx,
                               sinPhi * rx * std::cos(t) + cosPhi * ry * std::sin(t) + cy});
            }
        }

        SvgRaster::ContourSet ParsePathData(const std::string &d, const Mat2x3 &m)
        {
            SvgRaster::ContourSet contours;
            SvgRaster::Contour current;
            SvgRaster::Point cur{0, 0}, start{0, 0}, lastCtrl{0, 0};
            char lastCmd = 0, cmd = 0;

            auto pushLocal = [&](SvgRaster::Point p)
            { current.push_back(Apply(m, p.x, p.y)); };
            auto flushSubpath = [&]()
            { if (!current.empty()) contours.push_back(std::move(current)); current.clear(); };

            size_t i = 0;
            while (i < d.size())
            {
                while (i < d.size() && (std::isspace((unsigned char)d[i]) || d[i] == ','))
                    ++i;
                if (i >= d.size())
                    break;
                if (std::isalpha((unsigned char)d[i]))
                    cmd = d[i++];
                bool relative = std::islower((unsigned char)cmd);
                char upperCmd = static_cast<char>(std::toupper((unsigned char)cmd));

                auto readNum = [&]() -> float
                {
                    while (i < d.size() && (std::isspace((unsigned char)d[i]) || d[i] == ','))
                        ++i;
                    size_t s2 = i;
                    if (i < d.size() && (d[i] == '+' || d[i] == '-'))
                        ++i;
                    while (i < d.size() && (std::isdigit((unsigned char)d[i]) || d[i] == '.'))
                        ++i;
                    if (i < d.size() && (d[i] == 'e' || d[i] == 'E'))
                    {
                        ++i;
                        if (i < d.size() && (d[i] == '+' || d[i] == '-'))
                            ++i;
                        while (i < d.size() && std::isdigit((unsigned char)d[i]))
                            ++i;
                    }
                    return std::strtof(d.substr(s2, i - s2).c_str(), nullptr);
                };
                auto readFlag = [&]() -> bool
                {
                    while (i < d.size() && (std::isspace((unsigned char)d[i]) || d[i] == ','))
                        ++i;
                    return i < d.size() ? (d[i++] == '1') : false;
                };
                auto rel = [&](float x, float y)
                {
                    return relative ? SvgRaster::Point{cur.x + x, cur.y + y} : SvgRaster::Point{x, y};
                };

                switch (upperCmd)
                {
                case 'M':
                {
                    flushSubpath();
                    float x = readNum(), y = readNum();
                    cur = rel(x, y);
                    start = cur;
                    pushLocal(cur);
                    cmd = relative ? 'l' : 'L'; // subsequent pairs are implicit LineTo
                    break;
                }
                case 'L':
                {
                    cur = rel(readNum(), readNum());
                    pushLocal(cur);
                    break;
                }
                case 'H':
                {
                    float x = readNum();
                    cur = relative ? SvgRaster::Point{cur.x + x, cur.y} : SvgRaster::Point{x, cur.y};
                    pushLocal(cur);
                    break;
                }
                case 'V':
                {
                    float y = readNum();
                    cur = relative ? SvgRaster::Point{cur.x, cur.y + y} : SvgRaster::Point{cur.x, y};
                    pushLocal(cur);
                    break;
                }
                case 'C':
                {
                    SvgRaster::Point p1 = rel(readNum(), readNum()), p2 = rel(readNum(), readNum()), p3 = rel(readNum(), readNum());
                    SvgRaster::Contour local;
                    FlattenCubic(local, cur, p1, p2, p3);
                    for (auto &p : local)
                        current.push_back(Apply(m, p.x, p.y));
                    cur = p3;
                    lastCtrl = p2;
                    break;
                }
                case 'S':
                {
                    SvgRaster::Point p1 = (lastCmd == 'C' || lastCmd == 'S') ? SvgRaster::Point{2 * cur.x - lastCtrl.x, 2 * cur.y - lastCtrl.y} : cur;
                    SvgRaster::Point p2 = rel(readNum(), readNum()), p3 = rel(readNum(), readNum());
                    SvgRaster::Contour local;
                    FlattenCubic(local, cur, p1, p2, p3);
                    for (auto &p : local)
                        current.push_back(Apply(m, p.x, p.y));
                    cur = p3;
                    lastCtrl = p2;
                    break;
                }
                case 'Q':
                {
                    SvgRaster::Point p1 = rel(readNum(), readNum()), p2 = rel(readNum(), readNum());
                    SvgRaster::Contour local;
                    FlattenQuad(local, cur, p1, p2);
                    for (auto &p : local)
                        current.push_back(Apply(m, p.x, p.y));
                    cur = p2;
                    lastCtrl = p1;
                    break;
                }
                case 'T':
                {
                    SvgRaster::Point p1 = (lastCmd == 'Q' || lastCmd == 'T') ? SvgRaster::Point{2 * cur.x - lastCtrl.x, 2 * cur.y - lastCtrl.y} : cur;
                    SvgRaster::Point p2 = rel(readNum(), readNum());
                    SvgRaster::Contour local;
                    FlattenQuad(local, cur, p1, p2);
                    for (auto &p : local)
                        current.push_back(Apply(m, p.x, p.y));
                    cur = p2;
                    lastCtrl = p1;
                    break;
                }
                case 'A':
                {
                    float rx = readNum(), ry = readNum(), xRot = readNum();
                    bool largeArc = readFlag(), sweep = readFlag();
                    SvgRaster::Point end = rel(readNum(), readNum());
                    SvgRaster::Contour local;
                    FlattenArc(local, cur, rx, ry, xRot, largeArc, sweep, end);
                    for (auto &p : local)
                        current.push_back(Apply(m, p.x, p.y));
                    cur = end;
                    break;
                }
                case 'Z':
                {
                    cur = start;
                    pushLocal(cur);
                    flushSubpath();
                    break;
                }
                default:
                    i = d.size();
                    break; // unsupported command; stop rather than loop forever
                }
                lastCmd = upperCmd;
            }
            flushSubpath();
            return contours;
        }

        SvgRaster::ContourSet ShapeToContours(const SvgXml::Node &node, const Mat2x3 &m)
        {
            std::string tag = LocalName(node.tag);
            SvgRaster::ContourSet result;
            auto attrF = [&](const char *name, float def = 0.0f)
            {
                const std::string *v = node.Attr(name);
                return v ? std::strtof(v->c_str(), nullptr) : def;
            };

            if (tag == "rect")
            {
                float x = attrF("x"), y = attrF("y"), w = attrF("width"), h = attrF("height");
                if (w <= 0.0f || h <= 0.0f)
                    return result;
                SvgRaster::Contour c;
                c.push_back(Apply(m, x, y));
                c.push_back(Apply(m, x + w, y));
                c.push_back(Apply(m, x + w, y + h));
                c.push_back(Apply(m, x, y + h));
                result.push_back(std::move(c));
            }
            else if (tag == "circle" || tag == "ellipse")
            {
                float cx = attrF("cx"), cy = attrF("cy");
                float rx = tag == "circle" ? attrF("r") : attrF("rx");
                float ry = tag == "circle" ? attrF("r") : attrF("ry");
                if (rx <= 0.0f || ry <= 0.0f)
                    return result;
                SvgRaster::Contour c;
                constexpr int kSegments = 48;
                for (int i = 0; i < kSegments; ++i)
                {
                    float t = (2.0f * 3.14159265358979323846f * i) / kSegments;
                    c.push_back(Apply(m, cx + rx * std::cos(t), cy + ry * std::sin(t)));
                }
                result.push_back(std::move(c));
            }
            else if (tag == "line")
            {
                SvgRaster::Contour c;
                c.push_back(Apply(m, attrF("x1"), attrF("y1")));
                c.push_back(Apply(m, attrF("x2"), attrF("y2")));
                result.push_back(std::move(c)); // used for stroke only, fill is skipped below
            }
            else if (tag == "polyline" || tag == "polygon")
            {
                if (const std::string *pts = node.Attr("points"))
                {
                    auto nums = ParseNumberList(*pts);
                    SvgRaster::Contour c;
                    for (size_t i = 0; i + 1 < nums.size(); i += 2)
                        c.push_back(Apply(m, nums[i], nums[i + 1]));
                    result.push_back(std::move(c));
                }
            }
            else if (tag == "path")
            {
                if (const std::string *dAttr = node.Attr("d"))
                    result = ParsePathData(*dAttr, m);
            }
            return result;
        }

        void WalkAndDraw(const SvgXml::Node &node, Mat2x3 parentTransform, Style parentStyle,
                         SvgRaster::PixelBuffer &buffer)
        {
            std::string tag = LocalName(node.tag);
            if (tag == "defs" || tag == "symbol" || tag == "clipPath" || tag == "mask" ||
                tag == "linearGradient" || tag == "radialGradient" || tag == "style" || tag == "text")
                return; // unsupported; skipped rather than mis-rendered

            Mat2x3 transform = parentTransform;
            if (const std::string *t = node.Attr("transform"))
                transform = Multiply(parentTransform, ParseTransform(*t));

            Style style = ResolveStyle(node, parentStyle);

            bool isShape = (tag == "rect" || tag == "circle" || tag == "ellipse" || tag == "line" ||
                            tag == "polyline" || tag == "polygon" || tag == "path");
            if (isShape)
            {
                SvgRaster::ContourSet contours = ShapeToContours(node, transform);
                if (!contours.empty())
                {
                    if (style.fill && tag != "line")
                    {
                        SvgRaster::Rgba c = *style.fill;
                        c.a = static_cast<uint8_t>(c.a * style.opacity);
                        SvgRaster::FillContours(buffer, contours, c, style.evenOdd);
                    }
                    if (style.stroke && style.strokeWidth > 0.0f)
                    {
                        SvgRaster::Rgba c = *style.stroke;
                        c.a = static_cast<uint8_t>(c.a * style.opacity);
                        bool closed = (tag == "polygon" || tag == "rect" || tag == "circle" || tag == "ellipse" || tag == "path");
                        float scale = std::sqrt(std::abs(transform.a * transform.d - transform.b * transform.c));
                        SvgRaster::StrokeContours(buffer, contours, closed, style.strokeWidth * scale, c);
                    }
                }
            }

            for (const auto &child : node.children)
                WalkAndDraw(*child, transform, style, buffer);
        }

        std::string Base64Encode(const uint8_t *data, size_t len)
        {
            static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            out.reserve(((len + 2) / 3) * 4);
            size_t i = 0;
            while (i + 3 <= len)
            {
                uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
                out += table[(n >> 18) & 0x3f];
                out += table[(n >> 12) & 0x3f];
                out += table[(n >> 6) & 0x3f];
                out += table[n & 0x3f];
                i += 3;
            }
            size_t rem = len - i;
            if (rem == 1)
            {
                uint32_t n = data[i] << 16;
                out += table[(n >> 18) & 0x3f];
                out += table[(n >> 12) & 0x3f];
                out += "==";
            }
            else if (rem == 2)
            {
                uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
                out += table[(n >> 18) & 0x3f];
                out += table[(n >> 12) & 0x3f];
                out += table[(n >> 6) & 0x3f];
                out += '=';
            }
            return out;
        }

        // Minimal uncompressed 32bpp top-down BMP encoder.
        std::vector<uint8_t> EncodeBmp(const uint8_t *rgba, int width, int height)
        {
            const uint32_t headerSize = 14 + 40;
            const uint32_t imageSize = static_cast<uint32_t>(width) * height * 4;
            std::vector<uint8_t> out(headerSize + imageSize);
            auto put16 = [&](size_t off, uint16_t v)
            { std::memcpy(out.data() + off, &v, 2); };
            auto put32 = [&](size_t off, uint32_t v)
            { std::memcpy(out.data() + off, &v, 4); };

            out[0] = 'B';
            out[1] = 'M';
            put32(2, headerSize + imageSize);
            put32(6, 0);
            put32(10, headerSize);
            put32(14, 40);
            put32(18, static_cast<uint32_t>(width));
            put32(22, static_cast<uint32_t>(-height));
            put16(26, 1);
            put16(28, 32);
            put32(30, 0);
            put32(34, imageSize);
            put32(38, 2835);
            put32(42, 2835);
            put32(46, 0);
            put32(50, 0);

            uint8_t *dst = out.data() + headerSize;
            for (int p = 0; p < width * height; ++p)
            {
                dst[p * 4 + 0] = rgba[p * 4 + 2];
                dst[p * 4 + 1] = rgba[p * 4 + 1];
                dst[p * 4 + 2] = rgba[p * 4 + 0];
                dst[p * 4 + 3] = rgba[p * 4 + 3];
            }
            return out;
        }
    }
}
