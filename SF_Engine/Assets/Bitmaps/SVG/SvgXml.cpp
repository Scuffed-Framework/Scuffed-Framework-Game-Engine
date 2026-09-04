#include "SvgXml.hpp"
#include <cctype>

namespace SF::Engine::SvgXml
{
    namespace
    {
        struct Cursor
        {
            const std::string& s;
            size_t i = 0;

            bool Eof() const { return i >= s.size(); }
            char Peek() const { return Eof() ? '\0' : s[i]; }
            char Get() { return s[i++]; }

            void SkipWs()
            {
                while (!Eof() && std::isspace(static_cast<unsigned char>(Peek())))
                    ++i;
            }

            bool StartsWith(const char* lit) const
            {
                size_t n = 0;
                while (lit[n] != '\0')
                {
                    if (i + n >= s.size() || s[i + n] != lit[n])
                        return false;
                    ++n;
                }
                return true;
            }

            void SkipUntil(const char* lit)
            {
                while (!Eof() && !StartsWith(lit))
                    ++i;
                if (!Eof())
                    i += std::char_traits<char>::length(lit);
            }
        };

        std::string ParseName(Cursor& c)
        {
            std::string name;
            while (!c.Eof())
            {
                char ch = c.Peek();
                if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-' || ch == ':' || ch == '.')
                    name += c.Get();
                else
                    break;
            }
            return name;
        }

        std::string ParseQuoted(Cursor& c)
        {
            std::string value;
            if (c.Peek() != '"' && c.Peek() != '\'')
                return value;
            char quote = c.Get();
            while (!c.Eof() && c.Peek() != quote)
                value += c.Get();
            if (!c.Eof())
                c.Get();
            return value;
        }

        void ParseAttributes(Cursor& c, Node& node)
        {
            for (;;)
            {
                c.SkipWs();
                if (c.Eof() || c.Peek() == '>' || c.Peek() == '/')
                    return;
                std::string name = ParseName(c);
                if (name.empty())
                {
                    ++c.i; // malformed; skip a char rather than loop forever
                    continue;
                }
                c.SkipWs();
                std::string value;
                if (c.Peek() == '=')
                {
                    c.Get();
                    c.SkipWs();
                    value = ParseQuoted(c);
                }
                node.attributes.emplace(std::move(name), std::move(value));
            }
        }

        std::unique_ptr<Node> ParseElement(Cursor& c);

        void ParseChildren(Cursor& c, Node& node)
        {
            for (;;)
            {
                c.SkipWs();
                if (c.Eof())
                    return;
                if (c.StartsWith("</")) { c.SkipUntil(">"); return; }
                if (c.StartsWith("<!--")) { c.SkipUntil("-->"); continue; }
                if (c.StartsWith("<![CDATA[")) { c.SkipUntil("]]>"); continue; }
                if (c.StartsWith("<?")) { c.SkipUntil("?>"); continue; }
                if (c.StartsWith("<!")) { c.SkipUntil(">"); continue; }
                if (c.Peek() == '<')
                {
                    if (auto child = ParseElement(c))
                        node.children.push_back(std::move(child));
                    continue;
                }
                while (!c.Eof() && c.Peek() != '<') // text content; shapes don't need it
                    c.Get();
            }
        }

        std::unique_ptr<Node> ParseElement(Cursor& c)
        {
            if (c.Peek() != '<')
                return nullptr;
            c.Get();

            auto node = std::make_unique<Node>();
            node->tag = ParseName(c);
            if (node->tag.empty())
                return nullptr;

            ParseAttributes(c, *node);
            c.SkipWs();

            if (c.Peek() == '/')
            {
                c.Get();
                if (c.Peek() == '>') c.Get();
                return node;
            }
            if (c.Peek() == '>')
                c.Get();

            ParseChildren(c, *node);
            return node;
        }
    }

    std::unique_ptr<Node> Parse(const std::string& xml)
    {
        Cursor c{xml};
        for (;;)
        {
            c.SkipWs();
            if (c.Eof())
                return nullptr;
            if (c.StartsWith("<?")) { c.SkipUntil("?>"); continue; }
            if (c.StartsWith("<!--")) { c.SkipUntil("-->"); continue; }
            if (c.StartsWith("<!")) { c.SkipUntil(">"); continue; }
            if (c.Peek() == '<')
                return ParseElement(c);
            c.Get();
        }
    }
}