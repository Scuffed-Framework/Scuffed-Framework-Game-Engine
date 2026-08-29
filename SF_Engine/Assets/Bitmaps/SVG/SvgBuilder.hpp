#pragma once

#include <functional>
#include <string>
#include <utility>

#include <UtilityClasses/Patterns.hpp>
#include "BitmapSvg.hpp"

namespace SF::Engine
{
    class SvgGroupBuilder;

    template <typename Derived, typename Product>
    class SvgShapeBuilder : public BuilderPattern<Derived, Product>
    {
        using Base = BuilderPattern<Derived, Product>;

    public:
        Derived &Rect(Vec2 position, Vec2 size, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(SvgRect{position, size, 0.0f, 0.0f, std::move(style)});
            return Base::Self();
        }

        Derived &RoundedRect(Vec2 position, Vec2 size, float radius, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(SvgRect{position, size, radius, radius, std::move(style)});
            return Base::Self();
        }

        Derived &Circle(Vec2 center, float radius, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(SvgCircle{center, radius, std::move(style)});
            return Base::Self();
        }

        Derived &Ellipse(Vec2 center, Vec2 radius, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(SvgEllipse{center, radius, std::move(style)});
            return Base::Self();
        }

        Derived &Line(Vec2 start, Vec2 end, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(SvgLine{start, end, std::move(style)});
            return Base::Self();
        }

        Derived &Polyline(std::vector<Vec2> points, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(SvgPolyline{std::move(points), std::move(style)});
            return Base::Self();
        }

        Derived &Polygon(std::vector<Vec2> points, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(SvgPolygon{std::move(points), std::move(style)});
            return Base::Self();
        }

        Derived &Path(std::string pathData, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(SvgPath{std::move(pathData), std::move(style)});
            return Base::Self();
        }

        Derived &Text(Vec2 position, std::string content, float fontSize = 16.0f, SvgStyle style = {})
        {
            Base::m_Product.elements.push_back(
                SvgText{position, std::move(content), fontSize, "sans-serif", std::move(style)});
            return Base::Self();
        }

        // Compose a nested <g>: Group([](SvgGroupBuilder& g){ g.Rect(...).Circle(...); }, style)
        Derived &Group(std::function<void(SvgGroupBuilder &)> fn, SvgStyle style = {});
    };

    class SvgGroupBuilder : public SvgShapeBuilder<SvgGroupBuilder, SvgGroup>
    {
    };

    class SvgBuilder : public SvgShapeBuilder<SvgBuilder, SvgDocument>
    {
    public:
        SvgBuilder &Size(float width, float height)
        {
            m_Product.width = width;
            m_Product.height = height;
            return Self();
        }

        SvgBuilder &ViewBox(float minX, float minY, float width, float height)
        {
            m_Product.viewBox = std::array<float, 4>{minX, minY, width, height};
            return Self();
        }

        SvgBuilder &Title(std::string title)
        {
            m_Product.title = std::move(title);
            return Self();
        }
    };

    // Out-of-line: needs SvgGroupBuilder to be a complete type.
    template <typename Derived, typename Product>
    Derived &SvgShapeBuilder<Derived, Product>::Group(std::function<void(SvgGroupBuilder &)> fn, SvgStyle style)
    {
        SvgGroupBuilder groupBuilder;
        fn(groupBuilder);
        SvgGroup group = groupBuilder.Build();
        group.style = std::move(style);
        Base::m_Product.elements.push_back(std::make_unique<SvgGroup>(std::move(group)));
        return Base::Self();
    }

    // ---------------------------------------------------------------------
    // SvgPathBuilder — fluent construction of a <path> 'd' attribute.
    // Product is plain std::string so it can be handed straight to
    // SvgBuilder::Path(...) or stored/reused independently.
    // ---------------------------------------------------------------------

    class SvgPathBuilder : public BuilderPattern<SvgPathBuilder, std::string>
    {
    public:
        SvgPathBuilder &MoveTo(Vec2 point);
        SvgPathBuilder &LineTo(Vec2 point);
        SvgPathBuilder &HorizontalTo(float x);
        SvgPathBuilder &VerticalTo(float y);
        SvgPathBuilder &CubicTo(Vec2 control1, Vec2 control2, Vec2 end);
        SvgPathBuilder &SmoothCubicTo(Vec2 control2, Vec2 end);
        SvgPathBuilder &QuadTo(Vec2 control, Vec2 end);
        SvgPathBuilder &ArcTo(Vec2 radius, float xRotationDeg, bool largeArc, bool sweep, Vec2 end);
        SvgPathBuilder &Close();
    };
}
