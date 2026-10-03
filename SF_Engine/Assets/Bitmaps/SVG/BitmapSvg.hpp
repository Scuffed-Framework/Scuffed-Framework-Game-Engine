#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <Assets/Bitmaps/Bitmap.hpp>

namespace SF::Engine
{
    using namespace std;
    class SvgColor
    {
    public:
        SvgColor() = default;

        static SvgColor None();
        static SvgColor Named(string name);
        static SvgColor Rgb(uint8_t r, uint8_t g, uint8_t b);
        static SvgColor Rgba(uint8_t r, uint8_t g, uint8_t b, float a);

        [[nodiscard]] const string &ToString() const { return m_Value; }

    private:
        string m_Value = "#000000";
    };

    struct SvgStyle
    {
        optional<SvgColor> fill;
        optional<SvgColor> stroke;
        optional<float> strokeWidth;
        optional<float> opacity;
        optional<float> fillOpacity;
        optional<float> strokeOpacity;
        string strokeLinecap;  // "butt" | "round" | "square"
        string strokeLinejoin; // "miter" | "round" | "bevel"
        string transform;
        string id;
        string cssClass;

        SvgStyle &WithFill(SvgColor color)
        {
            fill = std::move(color);
            return *this;
        }
        SvgStyle &WithStroke(SvgColor color, float width = 1.0f)
        {
            stroke      = std::move(color);
            strokeWidth = width;
            return *this;
        }
        SvgStyle &WithOpacity(float value)
        {
            opacity = value;
            return *this;
        }
        SvgStyle &WithTransform(string value)
        {
            transform = std::move(value);
            return *this;
        }
        SvgStyle &WithId(string value)
        {
            id = std::move(value);
            return *this;
        }
        SvgStyle &WithClass(string value)
        {
            cssClass = std::move(value);
            return *this;
        }

        void WriteAttributes(ostream &os) const;
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
        vector<Vec2> points;
        SvgStyle style;
    };

    struct SvgPolygon
    {
        vector<Vec2> points;
        SvgStyle style;
    };

    struct SvgPath
    {
        string data; // the 'd' attribute; build with SvgPathBuilder
        SvgStyle style;
    };

    struct SvgText
    {
        Vec2 position{};
        string content;
        float fontSize    = 16.0f;
        string fontFamily = "sans-serif";
        SvgStyle style;
    };

    struct SvgGroup;

    using SvgElement = variant<SvgRect, SvgCircle, SvgEllipse, SvgLine, SvgPolyline, SvgPolygon, SvgPath, SvgText,
                               unique_ptr<SvgGroup>>;

    struct SvgGroup
    {
        SvgStyle style;
        vector<SvgElement> elements;
    };

    class SvgDocument
    {
    public:
        void Write(const filesystem::path &filename) const;

        float width  = 100.0f;
        float height = 100.0f;
        optional<array<float, 4>> viewBox; // minX, minY, width, height
        string title;
        vector<SvgElement> elements;
    };

    // Load: parsed and rasterized by plutosvg (RGBA8, straight alpha).
    // Write: embeds the bitmap as a base64 BMP <image> inside an SVG wrapper.
    class BitmapSvg : public Bitmap::Registrar<BitmapSvg>
    {
    public:
        static void Load(Bitmap &bitmap, const filesystem::path &filename);
        static void Write(const Bitmap &bitmap, const filesystem::path &filename);

        // Rasterizes SVG markup directly (embedded icons, generated documents, ...).
        // width/height == 0 uses the document's own size; if only one is given the other keeps the aspect ratio.
        // `svg` only needs to stay alive for the duration of the call.
        static void LoadFromMemory(Bitmap &bitmap, string_view svg, uint32_t width = 0, uint32_t height = 0);
        // todo: impl & take data from svgbuilder

    private:
        static inline bool registered = Register("svg", "SVG");
    };
} // namespace SF::Engine
