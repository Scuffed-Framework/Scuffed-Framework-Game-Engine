#pragma once
#include <Assets/Bitmaps/SVG/BitmapSvg.hpp>
#include <Rendering/RHI/Images/Image.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace SF::Engine
{
    namespace EmbeddedIcons
    {
        inline constexpr std::string_view kBlue = "#007FFF";

        // All icons are authored on a 256x256 canvas and rasterized by plutosvg at whatever size is requested.
        inline std::string Svg(std::string_view body)
        {
            std::string svg;
            svg.reserve(body.size() + 128);
            svg += R"(<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">)";
            svg += body;
            svg += "</svg>";
            return svg;
        }

        // Blue document with a folded corner; `glyph` is drawn on top in white.
        inline std::string FileIcon(std::string_view glyph)
        {
            std::string body;
            body += R"(<path d="M64 24h88l56 56v144a8 8 0 0 1-8 8H64a8 8 0 0 1-8-8V32a8 8 0 0 1 8-8z" fill=")";
            body += kBlue;
            body += R"("/>)";
            body += R"(<path d="M152 24v48a8 8 0 0 0 8 8h48z" fill="#ffffff" fill-opacity="0.35"/>)";
            body += glyph;
            return Svg(body);
        }

        inline std::string FolderSvg()
        {
            return Svg(R"svg(
<defs>
  <linearGradient id="front" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#FFD54A"/>
    <stop offset="1" stop-color="#FFB02C"/>
  </linearGradient>
</defs>
<path d="M24 56a16 16 0 0 1 16-16h60l24 24h92a16 16 0 0 1 16 16v128a16 16 0 0 1-16 16H40a16 16 0 0 1-16-16z"
      fill="#E8991A"/>
<path d="M24 104a16 16 0 0 1 16-16h176a16 16 0 0 1 16 16v104a16 16 0 0 1-16 16H40a16 16 0 0 1-16-16z"
      fill="url(#front)"/>
)svg");
        }

        // Plain document with text lines.
        inline std::string FileSvg()
        {
            return FileIcon(R"svg(
<path d="M88 128h88M88 160h88M88 192h56" fill="none" stroke="#ffffff" stroke-opacity="0.9"
      stroke-width="12" stroke-linecap="round"/>
)svg");
        }

        // Document with a plus.
        inline std::string NewSvg()
        {
            return FileIcon(R"svg(
<path d="M132 128v64M100 160h64" fill="none" stroke="#ffffff" stroke-width="16"
      stroke-linecap="round" stroke-linejoin="round"/>
)svg");
        }

        // Floppy disk.
        inline std::string SaveSvg()
        {
            return Svg(R"svg(
<path d="M48 32H176L224 80V208a16 16 0 0 1-16 16H48a16 16 0 0 1-16-16V48a16 16 0 0 1 16-16z" fill="#007FFF"/>
<rect x="80" y="32" width="88" height="56" fill="#ffffff" fill-opacity="0.9"/>
<rect x="128" y="44" width="24" height="36" rx="4" fill="#007FFF"/>
<rect x="64" y="128" width="128" height="80" rx="8" fill="#ffffff" fill-opacity="0.9"/>
)svg");
        }

        // Header file: H
        inline std::string HSvg()
        {
            return FileIcon(R"svg(
<path d="M104 124v72M160 124v72M104 160h56" fill="none" stroke="#ffffff" stroke-width="16"
      stroke-linecap="round" stroke-linejoin="round"/>
)svg");
        }

        // C++ header: HPP
        inline std::string HppSvg()
        {
            return FileIcon(R"svg(
<path d="M78 140v40M102 140v40M78 160h24
         M118 180v-40h14a10 10 0 0 1 0 20h-14
         M158 180v-40h14a10 10 0 0 1 0 20h-14" fill="none" stroke="#ffffff" stroke-width="10"
      stroke-linecap="round" stroke-linejoin="round"/>
)svg");
        }

        // C source: C
        inline std::string CSvg()
        {
            return FileIcon(R"svg(
<path d="M159.6 136.9A36 36 0 1 0 159.6 183.1" fill="none" stroke="#ffffff" stroke-width="16"
      stroke-linecap="round" stroke-linejoin="round"/>
)svg");
        }

        // C++ source: C++
        inline std::string CppSvg()
        {
            return FileIcon(R"svg(
<path d="M125.5 142A28 28 0 1 0 125.5 178
         M156 150v20M146 160h20M188 150v20M178 160h20" fill="none" stroke="#ffffff" stroke-width="12"
      stroke-linecap="round" stroke-linejoin="round"/>
)svg");
        }

        // Rasterizes the SVG at size x size (default 256) and uploads it as an Image2d.
        inline std::shared_ptr<Image2d> Construct(const std::string &svg, uint32_t size = 256)
        {
            auto bitmap = std::make_unique<Bitmap>();
            BitmapSvg::LoadFromMemory(*bitmap, svg, {size, size});
            return std::make_shared<Image2d>(std::move(bitmap));
        }
    } // namespace EmbeddedIcons

    // Same entry points as before; the optional size gives crisp icons at any resolution.
    inline std::shared_ptr<Image2d> GetSaveLogo(uint32_t size = 256)
    {
        return EmbeddedIcons::Construct(EmbeddedIcons::SaveSvg(), size);
    }
    inline std::shared_ptr<Image2d> GetNewLogo(uint32_t size = 256)
    {
        return EmbeddedIcons::Construct(EmbeddedIcons::NewSvg(), size);
    }
    inline std::shared_ptr<Image2d> GetHppLogo(uint32_t size = 256)
    {
        return EmbeddedIcons::Construct(EmbeddedIcons::HppSvg(), size);
    }
    inline std::shared_ptr<Image2d> GetHlogo(uint32_t size = 256)
    {
        return EmbeddedIcons::Construct(EmbeddedIcons::HSvg(), size);
    }
    inline std::shared_ptr<Image2d> GetFolderLogo(uint32_t size = 256)
    {
        return EmbeddedIcons::Construct(EmbeddedIcons::FolderSvg(), size);
    }
    inline std::shared_ptr<Image2d> GetFileLogo(uint32_t size = 256)
    {
        return EmbeddedIcons::Construct(EmbeddedIcons::FileSvg(), size);
    }
    inline std::shared_ptr<Image2d> GetCLogo(uint32_t size = 256)
    {
        return EmbeddedIcons::Construct(EmbeddedIcons::CSvg(), size);
    }
    inline std::shared_ptr<Image2d> GetCppLogo(uint32_t size = 256)
    {
        return EmbeddedIcons::Construct(EmbeddedIcons::CppSvg(), size);
    }
} // namespace SF::Engine
