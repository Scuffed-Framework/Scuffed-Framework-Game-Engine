#include "BitmapSvg.hpp"

#include <format>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <spdlog/spdlog.h>

namespace SF::Engine
{
    SvgColor SvgColor::None()
    {
        SvgColor c;
        c.m_Value = "none";
        return c;
    }

    SvgColor SvgColor::Named(std::string name)
    {
        SvgColor c;
        c.m_Value = std::move(name);
        return c;
    }

    SvgColor SvgColor::Rgb(uint8_t r, uint8_t g, uint8_t b)
    {
        SvgColor c;
        c.m_Value = std::format("#{:02x}{:02x}{:02x}", r, g, b);
        return c;
    }

    SvgColor SvgColor::Rgba(uint8_t r, uint8_t g, uint8_t b, float a)
    {
        SvgColor c;
        c.m_Value = std::format("rgba({}, {}, {}, {})", r, g, b, a);
        return c;
    }

    void SvgStyle::WriteAttributes(std::ostream &os) const
    {
        if (fill)
            os << std::format(R"( fill="{}")", fill->ToString());
        if (stroke)
            os << std::format(R"( stroke="{}")", stroke->ToString());
        if (strokeWidth)
            os << std::format(R"( stroke-width="{}")", *strokeWidth);
        if (opacity)
            os << std::format(R"( opacity="{}")", *opacity);
        if (fillOpacity)
            os << std::format(R"( fill-opacity="{}")", *fillOpacity);
        if (strokeOpacity)
            os << std::format(R"( stroke-opacity="{}")", *strokeOpacity);
        if (!strokeLinecap.empty())
            os << std::format(R"( stroke-linecap="{}")", strokeLinecap);
        if (!strokeLinejoin.empty())
            os << std::format(R"( stroke-linejoin="{}")", strokeLinejoin);
        if (!transform.empty())
            os << std::format(R"( transform="{}")", transform);
        if (!id.empty())
            os << std::format(R"( id="{}")", id);
        if (!cssClass.empty())
            os << std::format(R"( class="{}")", cssClass);
    }

    namespace
    {
        void WriteElement(std::ostream &os, const SvgElement &element); // fwd (groups recurse)

        void WriteShape(std::ostream &os, const SvgRect &rect)
        {
            os << std::format(R"(<rect x="{}" y="{}" width="{}" height="{}")",
                              rect.position.x, rect.position.y, rect.size.x, rect.size.y);
            if (rect.rx != 0.0f)
                os << std::format(R"( rx="{}")", rect.rx);
            if (rect.ry != 0.0f)
                os << std::format(R"( ry="{}")", rect.ry);
            rect.style.WriteAttributes(os);
            os << "/>\n";
        }

        void WriteShape(std::ostream &os, const SvgCircle &circle)
        {
            os << std::format(R"(<circle cx="{}" cy="{}" r="{}")",
                              circle.center.x, circle.center.y, circle.radius);
            circle.style.WriteAttributes(os);
            os << "/>\n";
        }

        void WriteShape(std::ostream &os, const SvgEllipse &ellipse)
        {
            os << std::format(R"(<ellipse cx="{}" cy="{}" rx="{}" ry="{}")",
                              ellipse.center.x, ellipse.center.y, ellipse.radius.x, ellipse.radius.y);
            ellipse.style.WriteAttributes(os);
            os << "/>\n";
        }

        void WriteShape(std::ostream &os, const SvgLine &line)
        {
            os << std::format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}")",
                              line.start.x, line.start.y, line.end.x, line.end.y);
            line.style.WriteAttributes(os);
            os << "/>\n";
        }

        void WritePoints(std::ostream &os, const std::vector<Vec2> &points)
        {
            os << R"( points=")";
            for (size_t i = 0; i < points.size(); ++i)
            {
                if (i != 0)
                    os << ' ';
                os << std::format("{},{}", points[i].x, points[i].y);
            }
            os << '"';
        }

        void WriteShape(std::ostream &os, const SvgPolyline &polyline)
        {
            os << "<polyline";
            WritePoints(os, polyline.points);
            polyline.style.WriteAttributes(os);
            os << "/>\n";
        }

        void WriteShape(std::ostream &os, const SvgPolygon &polygon)
        {
            os << "<polygon";
            WritePoints(os, polygon.points);
            polygon.style.WriteAttributes(os);
            os << "/>\n";
        }

        void WriteShape(std::ostream &os, const SvgPath &path)
        {
            os << std::format(R"(<path d="{}")", path.data);
            path.style.WriteAttributes(os);
            os << "/>\n";
        }

        void WriteShape(std::ostream &os, const SvgText &text)
        {
            os << std::format(R"(<text x="{}" y="{}" font-size="{}" font-family="{}")",
                              text.position.x, text.position.y, text.fontSize, text.fontFamily);
            text.style.WriteAttributes(os);
            os << '>' << text.content << "</text>\n";
        }

        void WriteShape(std::ostream &os, const std::unique_ptr<SvgGroup> &group)
        {
            os << "<g";
            group->style.WriteAttributes(os);
            os << ">\n";
            for (const SvgElement &child : group->elements)
                WriteElement(os, child);
            os << "</g>\n";
        }

        void WriteElement(std::ostream &os, const SvgElement &element)
        {
            std::visit([&os](const auto &shape)
                       { WriteShape(os, shape); }, element);
        }
    }

    void SvgDocument::Write(const std::filesystem::path &filename) const
    {
        std::ofstream file(filename, std::ios::out | std::ios::trunc);
        if (!file)
            throw std::runtime_error("SvgDocument::Write: failed to open " + filename.string());

        file << R"(<?xml version="1.0" encoding="UTF-8"?>)" << '\n';
        file << std::format(R"(<svg xmlns="http://www.w3.org/2000/svg" width="{}" height="{}")",
                            width, height);
        if (viewBox)
        {
            const auto &[minX, minY, w, h] = *viewBox;
            file << std::format(R"( viewBox="{} {} {} {}")", minX, minY, w, h);
        }
        file << ">\n";

        if (!title.empty())
            file << "<title>" << title << "</title>\n";

        for (const SvgElement &element : elements)
            WriteElement(file, element);

        file << "</svg>\n";
    }

    void BitmapSvg::Load(Bitmap &bitmap, const std::filesystem::path &filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file)
            throw std::runtime_error("BitmapSvg::Load: failed to open " + filename.string());
        std::string xml((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        auto root = SvgXml::Parse(xml);
        if (!root || LocalName(root->tag) != "svg")
            throw std::runtime_error("BitmapSvg::Load: not a valid SVG file: " + filename.string());

        float docWidth = 0.0f, docHeight = 0.0f;
        if (const std::string *w = root->Attr("width"))
            docWidth = std::strtof(w->c_str(), nullptr);
        if (const std::string *h = root->Attr("height"))
            docHeight = std::strtof(h->c_str(), nullptr);

        float vbMinX = 0, vbMinY = 0, vbW = 0, vbH = 0;
        bool hasViewBox = false;
        if (const std::string *vb = root->Attr("viewBox"))
        {
            auto nums = ParseNumberList(*vb);
            if (nums.size() == 4)
            {
                vbMinX = nums[0];
                vbMinY = nums[1];
                vbW = nums[2];
                vbH = nums[3];
                hasViewBox = true;
            }
        }
        if (docWidth <= 0.0f)
            docWidth = hasViewBox ? vbW : 256.0f;
        if (docHeight <= 0.0f)
            docHeight = hasViewBox ? vbH : 256.0f;

        int pixelW = std::max(1, static_cast<int>(std::round(docWidth)));
        int pixelH = std::max(1, static_cast<int>(std::round(docHeight)));

        Mat2x3 rootTransform;
        if (hasViewBox && vbW > 0.0f && vbH > 0.0f)
        {
            float sx = pixelW / vbW, sy = pixelH / vbH;
            rootTransform = {sx, 0, 0, sy, -vbMinX * sx, -vbMinY * sy};
        }

        SvgRaster::PixelBuffer buffer;
        buffer.Resize(pixelW, pixelH);

        Style rootStyle;
        for (const auto &child : root->children)
            WalkAndDraw(*child, rootTransform, rootStyle, buffer);

        // buffer.pixels is already tightly-packed RGBA8 — same layout Bitmap expects at bpp=4.
        const uint32_t byteCount = static_cast<uint32_t>(buffer.pixels.size() * sizeof(SvgRaster::Rgba));
        auto pixelData = std::make_unique<uint8_t[]>(byteCount);
        std::memcpy(pixelData.get(), buffer.pixels.data(), byteCount);

        bitmap.SetSize(UVec2{static_cast<uint32_t>(pixelW), static_cast<uint32_t>(pixelH)});
        bitmap.SetBytesPerPixel(4);
        bitmap.SetData(std::move(pixelData));
        bitmap.SetFilename(filename);
    }
    void BitmapSvg::Write(const Bitmap &bitmap, const std::filesystem::path &filename)
    {
        const UVec2 &size = bitmap.GetSize();
        uint32_t w = size.x, h = size.y;
        uint32_t bpp = bitmap.GetBytesPerPixel();
        const uint8_t *src = bitmap.GetData().get();
        if (!src || w == 0 || h == 0)
            throw std::runtime_error("BitmapSvg::Write: bitmap has no pixel data");

        // Normalize to RGBA8 — EncodeBmp only understands that layout.>
        std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
        for (uint32_t p = 0; p < w * h; ++p)
        {
            const uint8_t *s = src + p * bpp;
            uint8_t *d = rgba.data() + p * 4;
            switch (bpp)
            {
            case 4:
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d[3] = s[3];
                break;
            case 3:
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d[3] = 255;
                break;
            case 1:
                d[0] = d[1] = d[2] = s[0];
                d[3] = 255;
                break;
            default:
                throw std::runtime_error("BitmapSvg::Write: unsupported bytes-per-pixel " + std::to_string(bpp));
            }
        }

        std::vector<uint8_t> bmp = EncodeBmp(rgba.data(), static_cast<int>(w), static_cast<int>(h));
        std::string b64 = Base64Encode(bmp.data(), bmp.size());

        std::ofstream file(filename, std::ios::out | std::ios::trunc);
        if (!file)
            throw std::runtime_error("BitmapSvg::Write: failed to open " + filename.string());

        file << R"(<?xml version="1.0" encoding="UTF-8"?>)" << '\n';
        file << std::format(
                    R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="{}" height="{}" viewBox="0 0 {} {}">)",
                    w, h, w, h)
             << '\n';
        file << std::format(R"(<image width="{}" height="{}" xlink:href="data:image/bmp;base64,{}"/>)", w, h, b64) << '\n';
        file << "</svg>\n";
    }
}
