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

        const std::string &ToString() const { return m_Value; }

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
            stroke      = std::move(color);
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
        float fontSize         = 16.0f;
        std::string fontFamily = "sans-serif";
        SvgStyle style;
    };

    struct SvgGroup;

    using SvgElement = std::variant<SvgRect, SvgCircle, SvgEllipse, SvgLine, SvgPolyline, SvgPolygon, SvgPath, SvgText,
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

        float width  = 100.0f;
        float height = 100.0f;
        std::optional<std::array<float, 4>> viewBox; // minX, minY, width, height
        std::string title;
        std::vector<SvgElement> elements;
    };

    // Load: parsed and rasterized by plutosvg (RGBA8, straight alpha).
    // Write: embeds the bitmap as a base64 BMP <image> inside an SVG wrapper.
    class BitmapSvg : public Bitmap::Registrar<BitmapSvg>
    {
    public:
        static void Load(Bitmap &bitmap, const std::filesystem::path &filename);
        static void Write(const Bitmap &bitmap, const std::filesystem::path &filename);
        // todo: impl & take data from svgbuilder

    private:
        static inline bool registered = Register("svg", "SVG");
    };
} // namespace SF::Engine
