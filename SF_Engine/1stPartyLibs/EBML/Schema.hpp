#pragma once

#include "Element.hpp"
#include "Identifier.hpp"
#include "VINT.hpp"

#include <initializer_list>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace SF::EBML
{

    class Schema
    {
    public:
        void set(Identifier id, Element::Kind kind) { m_map[id.value()] = kind; }

        Element::Kind classify(Identifier id) const
        {
            auto it = m_map.find(id.value());
            return it == m_map.end() ? Element::Kind::Binary : it->second;
        }

        // Registers that `child` may appear as a direct child of `parent`.
        // This is what lets the parser know where an *unknown-size* master
        // element ends: it stops consuming children as soon as it sees an ID
        // that isn't registered as one of its children (a sibling, or a
        // descendant of some ancestor, instead). Needed for formats like
        // Matroska where Segment/Cluster are routinely written with unknown
        // size.
        void add_child(Identifier parent, Identifier child) { m_children[parent.value()].insert(child.value()); }

        void add_children(Identifier parent, std::initializer_list<Identifier> children)
        {
            auto &set = m_children[parent.value()];
            for (auto c : children)
                set.insert(c.value());
        }

        // True if `child` is a registered child of `parent`. If `parent` has
        // no children registered at all, this returns true unconditionally:
        // schemas that don't describe hierarchy fall back to "an unknown-size
        // element under this parent consumes everything until the enclosing
        // data runs out", which is the best that can be done without more
        // information.
        bool is_valid_child(Identifier parent, Identifier child) const
        {
            auto it = m_children.find(parent.value());
            if (it == m_children.end())
                return true;
            return it->second.count(child.value()) != 0;
        }

    private:
        std::unordered_map<std::uint32_t, Element::Kind> m_map;
        std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> m_children;
    };

    class ParseError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    struct ParseResult
    {
        Element element;
        std::size_t consumed;
    };

    inline ParseResult parse_element(std::span<const byte> data, const Schema &schema, int maxDepth = 64)
    {
        if (maxDepth <= 0)
            throw ParseError("EBML nesting exceeds max depth");

        auto id = Identifier::decode(data);
        if (!id)
            throw ParseError("invalid or truncated EBML element ID");

        auto afterId = data.subspan(id->length());
        auto size = decode_size(afterId);
        if (!size)
            throw ParseError("invalid or truncated EBML element size");

        auto afterSize = afterId.subspan(size->length);
        const Element::Kind kind = schema.classify(*id);

        if (size->unknown)
        {
            // Only master elements can sensibly have unknown size: the reader
            // has to interpret the body as a sequence of child elements in
            // order to know where it ends at all.
            if (kind != Element::Kind::Master)
                throw ParseError("unknown-size is only supported for master elements");

            Element element = Element::make_master(*id);
            std::span<const byte> cursor = afterSize;
            std::size_t bodyConsumed = 0;
            while (!cursor.empty())
            {
                // Peek the next child's ID (without consuming it yet) to
                // decide whether it still belongs to this element. Parsing
                // stops - without error - at the first ID that isn't a
                // registered child of *id (see Schema::is_valid_child), or
                // that fails to decode at all (treated as end of data, e.g.
                // trailing padding/garbage).
                auto peekId = Identifier::decode(cursor);
                if (!peekId || !schema.is_valid_child(*id, *peekId))
                    break;

                auto child = parse_element(cursor, schema, maxDepth - 1);
                element.add(std::move(child.element));
                cursor = cursor.subspan(child.consumed);
                bodyConsumed += child.consumed;
            }
            const std::size_t consumed = id->length() + size->length + bodyConsumed;
            return {std::move(element), consumed};
        }

        if (afterSize.size() < size->value)
            throw ParseError("EBML element body runs past end of input");

        auto body = afterSize.first(static_cast<std::size_t>(size->value));
        const std::size_t consumed = id->length() + size->length + static_cast<std::size_t>(size->value);

        if (kind == Element::Kind::Master)
        {
            Element element = Element::make_master(*id);
            std::span<const byte> cursor = body;
            while (!cursor.empty())
            {
                auto child = parse_element(cursor, schema, maxDepth - 1);
                element.add(std::move(child.element));
                cursor = cursor.subspan(child.consumed);
            }
            return {std::move(element), consumed};
        }

        return {Element::make_leaf(*id, kind, {body.begin(), body.end()}), consumed};
    }

    inline ElementList parse_stream(std::span<const byte> data, const Schema &schema)
    {
        ElementList out;
        std::span<const byte> cursor = data;
        while (!cursor.empty())
        {
            auto [element, consumed] = parse_element(cursor, schema);
            out.push_back(std::move(element));
            cursor = cursor.subspan(consumed);
        }
        return out;
    }

} // namespace SF::EBML