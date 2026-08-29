#include "SvgBuilder.hpp"

#include <format>

namespace SF::Engine
{
    namespace
    {
        void Append(std::string &out, char command, std::initializer_list<float> args)
        {
            if (!out.empty())
                out += ' ';
            out += command;
            for (float arg : args)
                out += std::format(" {}", arg);
        }
    }

    SvgPathBuilder &SvgPathBuilder::MoveTo(Vec2 point)
    {
        Append(m_Product, 'M', {point.x, point.y});
        return Self();
    }

    SvgPathBuilder &SvgPathBuilder::LineTo(Vec2 point)
    {
        Append(m_Product, 'L', {point.x, point.y});
        return Self();
    }

    SvgPathBuilder &SvgPathBuilder::HorizontalTo(float x)
    {
        Append(m_Product, 'H', {x});
        return Self();
    }

    SvgPathBuilder &SvgPathBuilder::VerticalTo(float y)
    {
        Append(m_Product, 'V', {y});
        return Self();
    }

    SvgPathBuilder &SvgPathBuilder::CubicTo(Vec2 control1, Vec2 control2, Vec2 end)
    {
        Append(m_Product, 'C', {control1.x, control1.y, control2.x, control2.y, end.x, end.y});
        return Self();
    }

    SvgPathBuilder &SvgPathBuilder::SmoothCubicTo(Vec2 control2, Vec2 end)
    {
        Append(m_Product, 'S', {control2.x, control2.y, end.x, end.y});
        return Self();
    }

    SvgPathBuilder &SvgPathBuilder::QuadTo(Vec2 control, Vec2 end)
    {
        Append(m_Product, 'Q', {control.x, control.y, end.x, end.y});
        return Self();
    }

    SvgPathBuilder &SvgPathBuilder::ArcTo(Vec2 radius, float xRotationDeg, bool largeArc, bool sweep, Vec2 end)
    {
        if (!m_Product.empty())
            m_Product += ' ';
        m_Product += 'A';
        m_Product += std::format(" {} {} {} {} {} {} {}",
                                  radius.x, radius.y, xRotationDeg,
                                  largeArc ? 1 : 0, sweep ? 1 : 0, end.x, end.y);
        return Self();
    }

    SvgPathBuilder &SvgPathBuilder::Close()
    {
        if (!m_Product.empty())
            m_Product += ' ';
        m_Product += 'Z';
        return Self();
    }
}
