#include "Color.hpp"
#include <algorithm>

namespace SF::Engine
{
    // Predefined colors
    const Color Color::Clear(0.0f, 0.0f, 0.0f, 0.0f);
    const Color Color::Black(0.0f, 0.0f, 0.0f, 1.0f);
    const Color Color::White(1.0f, 1.0f, 1.0f, 1.0f);
    const Color Color::Red(1.0f, 0.0f, 0.0f, 1.0f);
    const Color Color::Green(0.0f, 1.0f, 0.0f, 1.0f);
    const Color Color::Blue(0.0f, 0.0f, 1.0f, 1.0f);
    const Color Color::Yellow(1.0f, 1.0f, 0.0f, 1.0f);
    const Color Color::Cyan(0.0f, 1.0f, 1.0f, 1.0f);
    const Color Color::Magenta(1.0f, 0.0f, 1.0f, 1.0f);
    const Color Color::Orange(1.0f, 0.647f, 0.0f, 1.0f);
    const Color Color::Purple(0.5f, 0.0f, 0.5f, 1.0f);
    const Color Color::Pink(1.0f, 0.753f, 0.796f, 1.0f);
    const Color Color::Brown(0.647f, 0.165f, 0.165f, 1.0f);
    const Color Color::Gray(0.5f, 0.5f, 0.5f, 1.0f);
    const Color Color::LightGray(0.827f, 0.827f, 0.827f, 1.0f);
    const Color Color::DarkGray(0.663f, 0.663f, 0.663f, 1.0f);

    // Web colors
    const Color Color::Maroon(0.502f, 0.0f, 0.0f, 1.0f);
    const Color Color::Olive(0.502f, 0.502f, 0.0f, 1.0f);
    const Color Color::Lime(0.0f, 1.0f, 0.0f, 1.0f);
    const Color Color::Aqua(0.0f, 1.0f, 1.0f, 1.0f);
    const Color Color::Teal(0.0f, 0.502f, 0.502f, 1.0f);
    const Color Color::Navy(0.0f, 0.0f, 0.502f, 1.0f);
    const Color Color::Fuchsia(1.0f, 0.0f, 1.0f, 1.0f);
    const Color Color::Silver(0.753f, 0.753f, 0.753f, 1.0f);

    Color Color::FromHSV(float hue, float saturation, float value, float alpha)
    {
        hue = Maths::WrapDegrees(hue) / 60.0f;
        saturation = Maths::Saturate(saturation);
        value = Maths::Saturate(value);

        float c = value * saturation;
        float x = c * (1.0f - std::abs(std::fmod(hue, 2.0f) - 1.0f));
        float m = value - c;

        float r = 0, g = 0, b = 0;

        if (hue < 1.0f)
        {
            r = c;
            g = x;
            b = 0;
        }
        else if (hue < 2.0f)
        {
            r = x;
            g = c;
            b = 0;
        }
        else if (hue < 3.0f)
        {
            r = 0;
            g = c;
            b = x;
        }
        else if (hue < 4.0f)
        {
            r = 0;
            g = x;
            b = c;
        }
        else if (hue < 5.0f)
        {
            r = x;
            g = 0;
            b = c;
        }
        else
        {
            r = c;
            g = 0;
            b = x;
        }

        return Color(r + m, g + m, b + m, alpha);
    }

    Color Color::FromHSL(float hue, float saturation, float lightness, float alpha)
    {
        hue = Maths::WrapDegrees(hue) / 60.0f;
        saturation = Maths::Saturate(saturation);
        lightness = Maths::Saturate(lightness);

        float c = (1.0f - std::abs(2.0f * lightness - 1.0f)) * saturation;
        float x = c * (1.0f - std::abs(std::fmod(hue, 2.0f) - 1.0f));
        float m = lightness - c / 2.0f;

        float r = 0, g = 0, b = 0;

        if (hue < 1.0f)
        {
            r = c;
            g = x;
            b = 0;
        }
        else if (hue < 2.0f)
        {
            r = x;
            g = c;
            b = 0;
        }
        else if (hue < 3.0f)
        {
            r = 0;
            g = c;
            b = x;
        }
        else if (hue < 4.0f)
        {
            r = 0;
            g = x;
            b = c;
        }
        else if (hue < 5.0f)
        {
            r = x;
            g = 0;
            b = c;
        }
        else
        {
            r = c;
            g = 0;
            b = x;
        }

        return Color(r + m, g + m, b + m, alpha);
    }

    std::array<float, 3> Color::ToHSV() const
    {
        float cMax = std::max({r, g, b});
        float cMin = std::min({r, g, b});
        float delta = cMax - cMin;

        float h = 0.0f;
        if (delta > 0.0f)
        {
            if (cMax == r)
                h = 60.0f * std::fmod((g - b) / delta, 6.0f);
            else if (cMax == g)
                h = 60.0f * ((b - r) / delta + 2.0f);
            else
                h = 60.0f * ((r - g) / delta + 4.0f);
        }

        float s = (cMax > 0.0f) ? (delta / cMax) : 0.0f;
        float v = cMax;

        return {Maths::WrapDegrees(h), s, v};
    }

    std::array<float, 3> Color::ToHSL() const
    {
        float cMax = std::max({r, g, b});
        float cMin = std::min({r, g, b});
        float delta = cMax - cMin;

        float h = 0.0f;
        if (delta > 0.0f)
        {
            if (cMax == r)
                h = 60.0f * std::fmod((g - b) / delta, 6.0f);
            else if (cMax == g)
                h = 60.0f * ((b - r) / delta + 2.0f);
            else
                h = 60.0f * ((r - g) / delta + 4.0f);
        }

        float l = (cMax + cMin) / 2.0f;
        float s = (delta > 0.0f) ? (delta / (1.0f - std::abs(2.0f * l - 1.0f))) : 0.0f;

        return {Maths::WrapDegrees(h), s, l};
    }
}