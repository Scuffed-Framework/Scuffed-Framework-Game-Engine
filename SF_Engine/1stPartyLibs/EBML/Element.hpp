#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Identifier.hpp"

namespace SF::EBML
{
    using namespace std;
    using byte = uint8_t;

    // Forward declaration
    class Element;

    // Thread-safe element list with copy-on-write semantics
    class ElementList
    {
        mutable shared_mutex mutex_;
        vector<Element> elements_;

    public:
        // NOTE: Element and ElementList are mutually referential (Element holds
        // an ElementList of children; ElementList holds a vector<Element>).
        // Any method here that needs to construct/copy/inspect an Element must
        // be defined out-of-line, after Element's definition later in this
        // file; Element is only forward-declared at this point. Methods that
        // don't touch Element itself (size/empty/clear, and the erase_if
        // template, whose instantiation is deferred until use) can stay inline.
        ElementList();
        ElementList(const ElementList &other);
        ElementList(ElementList &&other) noexcept;
        ElementList &operator=(const ElementList &other);

        void push_back(Element element);
        void insert(size_t pos, Element element);

        template<typename Predicate>
        void erase_if(Predicate pred);

        size_t size() const;

        bool empty() const
        {
            shared_lock lock(mutex_);
            return elements_.empty();
        }

        // Find with shared lock
        const Element *find(Identifier id) const;

        // Non-const find with unique lock
        Element *find_mutable(Identifier id);

        // Iterator support with locking
        class const_iterator
        {
            vector<Element>::const_iterator it_;
            const ElementList *parent_;

        public:
            const_iterator(typename vector<Element>::const_iterator it, const ElementList *parent) :
                it_(it), parent_(parent)
            {
            }

            const Element &operator*() const { return *it_; }
            const Element *operator->() const { return &*it_; }

            const_iterator &operator++();
            const_iterator operator++(int);

            bool operator==(const const_iterator &other) const { return it_ == other.it_; }
            bool operator!=(const const_iterator &other) const { return it_ != other.it_; }
        };

        const_iterator begin() const;
        const_iterator end() const;

        // Range-based access with lock (returns snapshot)
        vector<Element> snapshot() const;

        // Clear all elements
        void clear();
    };

    // Decode functions
    inline uint64_t decode_uint(span<const byte> data)
    {
        if (data.size() > 8)
            throw runtime_error("EBML integer element must be at most 8 bytes");
        uint64_t v = 0;
        for (byte b: data)
            v = (v << 8) | b;
        return v;
    }

    inline int64_t decode_int(span<const byte> data)
    {
        uint64_t v = decode_uint(data);
        if (!data.empty() && (data[0] & 0x80) && data.size() < 8)
            v |= (~uint64_t{0}) << (data.size() * 8);
        return static_cast<int64_t>(v);
    }

    inline double decode_float(span<const byte> data)
    {
        if (data.size() == 4)
        {
            auto bits = static_cast<uint32_t>(decode_uint(data));
            float f;
            memcpy(&f, &bits, 4);
            return f;
        }
        if (data.size() == 8)
        {
            uint64_t bits = decode_uint(data);
            double d;
            memcpy(&d, &bits, 8);
            return d;
        }
        if (data.empty())
            return 0.0;
        throw runtime_error("EBML float element must be 0, 4 or 8 bytes");
    }

    inline vector<byte> encode_uint(uint64_t v)
    {
        vector<byte> out;
        do
        {
            out.insert(out.begin(), static_cast<byte>(v & 0xFF));
            v >>= 8;
        } while (v);
        return out;
    }

    inline vector<byte> encode_int(int64_t v)
    {
        vector<byte> out;
        auto u        = static_cast<uint64_t>(v);
        bool negative = v < 0;
        while (true)
        {
            out.insert(out.begin(), static_cast<byte>(u & 0xFF));
            bool leadingSignOk = negative ? (out.front() & 0x80) != 0 : (out.front() & 0x80) == 0;
            uint64_t next      = negative ? (u >> 8) | (~uint64_t{0} << 56) : (u >> 8);
            if (leadingSignOk && (negative ? next == (~uint64_t{0}) : next == 0))
                break;
            u = next;
        }
        return out;
    }

    inline vector<byte> encode_float(double v)
    {
        uint64_t bits;
        memcpy(&bits, &v, 8);
        auto out = encode_uint(bits);
        while (out.size() < 8)
            out.insert(out.begin(), 0);
        return out;
    }

    inline vector<byte> encode_float32(float v)
    {
        uint32_t bits;
        memcpy(&bits, &v, 4);
        auto out = encode_uint(bits);
        while (out.size() < 4)
            out.insert(out.begin(), 0);
        return out;
    }

    inline constexpr int64_t kDateEpochOffsetNs = 978307200LL * 1'000'000'000LL;

    // Main Element class
    class Element
    {
    public:
        enum class Kind
        {
            Master,
            UInt,
            Int,
            Float,
            String,
            Utf8,
            Date,
            Binary,
            Unknown
        };

        static constexpr string_view kind_name(Kind k)
        {
            switch (k)
            {
                case Kind::Master:
                    return "Master";
                case Kind::UInt:
                    return "UInt";
                case Kind::Int:
                    return "Int";
                case Kind::Float:
                    return "Float";
                case Kind::String:
                    return "String";
                case Kind::Utf8:
                    return "Utf8";
                case Kind::Date:
                    return "Date";
                case Kind::Binary:
                    return "Binary";
                default:
                    return "Unknown";
            }
        }

    private:
        Identifier m_id;
        Kind m_kind = Kind::Binary;
        mutable shared_mutex mutex_;
        variant<ElementList, vector<byte>> m_payload;

    public:
        Element() = default;

        Element(Identifier id, Kind kind, ElementList children) : m_id(id), m_kind(kind), m_payload(std::move(children))
        {
        }

        Element(Identifier id, Kind kind, vector<byte> data) : m_id(id), m_kind(kind), m_payload(std::move(data)) {}

        // Copy constructor (thread-safe)
        Element(const Element &other) : m_id(other.m_id), m_kind(other.m_kind)
        {
            shared_lock lock(other.mutex_);
            m_payload = other.m_payload;
        }

        // std::move constructor
        Element(Element &&other) noexcept :
            m_id(other.m_id), m_kind(other.m_kind), m_payload(std::move(other.m_payload))
        {
        }

        // Assignment operators
        Element &operator=(const Element &other)
        {
            if (this != &other)
            {
                unique_lock lock1(mutex_, defer_lock);
                shared_lock lock2(other.mutex_, defer_lock);
                lock(lock1, lock2);
                m_id      = other.m_id;
                m_kind    = other.m_kind;
                m_payload = other.m_payload;
            }
            return *this;
        }

        Element &operator=(Element &&other) noexcept
        {
            if (this != &other)
            {
                m_id      = other.m_id;
                m_kind    = other.m_kind;
                m_payload = std::move(other.m_payload);
            }
            return *this;
        }

        // Accessors
        Identifier id() const { return m_id; }
        Kind kind() const { return m_kind; }
        bool is_master() const { return m_kind == Kind::Master; }

        // Children access with locking
        ElementList &children() { return get<ElementList>(m_payload); }

        const ElementList &children() const { return get<ElementList>(m_payload); }

        void add(Element child) { children().push_back(std::move(child)); }

        const Element *find(Identifier childId) const { return children().find(childId); }

        Element *find_mutable(Identifier childId) { return children().find_mutable(childId); }

        // Raw data access
        vector<byte> &raw() { return get<vector<byte>>(m_payload); }

        const vector<byte> &raw() const { return get<vector<byte>>(m_payload); }

        // Type-safe accessors with validation - FIXED: using static decode functions
        uint64_t as_uint() const
        {
            if (m_kind != Kind::UInt)
                throw runtime_error("Element is not a UInt");
            return decode_uint(raw());
        }

        int64_t as_int() const
        {
            if (m_kind != Kind::Int)
                throw runtime_error("Element is not an Int");
            return decode_int(raw());
        }

        double as_float() const
        {
            if (m_kind != Kind::Float)
                throw runtime_error("Element is not a Float");
            return decode_float(raw());
        }

        string as_string() const
        {
            if (m_kind != Kind::String && m_kind != Kind::Utf8)
                throw runtime_error("Element is not a String or Utf8");
            return {raw().begin(), raw().end()};
        }

        chrono::sys_time<chrono::nanoseconds> as_date() const
        {
            if (m_kind != Kind::Date)
                throw runtime_error("Element is not a Date");
            int64_t ns = decode_int(raw());
            return chrono::sys_time<chrono::nanoseconds>{chrono::nanoseconds{ns + kDateEpochOffsetNs}};
        }

        // Factory methods
        static Element make_master(Identifier id) { return {id, Kind::Master, ElementList{}}; }

        static Element make_uint(Identifier id, uint64_t v) { return {id, Kind::UInt, encode_uint(v)}; }

        static Element make_int(Identifier id, int64_t v) { return {id, Kind::Int, encode_int(v)}; }

        static Element make_float(Identifier id, double v) { return {id, Kind::Float, encode_float(v)}; }

        static Element make_float32(Identifier id, float v) { return {id, Kind::Float, encode_float32(v)}; }

        static Element make_date(Identifier id, chrono::sys_time<chrono::nanoseconds> tp)
        {
            int64_t ns = tp.time_since_epoch().count() - kDateEpochOffsetNs;
            return {id, Kind::Date, encode_int(ns)};
        }

        static Element make_string(Identifier id, string_view v)
        {
            return {id, Kind::String, vector<byte>(v.begin(), v.end())};
        }

        static Element make_utf8(Identifier id, string_view v)
        {
            return {id, Kind::Utf8, vector<byte>(v.begin(), v.end())};
        }

        static Element make_binary(Identifier id, vector<byte> v) { return {id, Kind::Binary, std::move(v)}; }

        static Element make_leaf(Identifier id, Kind kind, vector<byte> data) { return {id, kind, std::move(data)}; }

        // Validation
        bool validate() const
        {
            if (m_kind == Kind::Master)
            {
                for (const auto &child: children())
                    if (!child.validate())
                        return false;
            } else if (m_kind == Kind::Float)
            {
                if (raw().size() != 4 && raw().size() != 8)
                    return false;
            } else if (m_kind == Kind::Date || m_kind == Kind::UInt || m_kind == Kind::Int)
            {
                if (raw().empty() || raw().size() > 8)
                    return false;
            }
            return true;
        }
    };

    inline ElementList::ElementList() = default;

    inline ElementList::ElementList(const ElementList &other)
    {
        shared_lock lock(other.mutex_);
        elements_ = other.elements_;
    }

    inline ElementList::ElementList(ElementList &&other) noexcept
    {
        unique_lock lock(other.mutex_);
        elements_ = std::move(other.elements_);
    }

    inline ElementList &ElementList::operator=(const ElementList &other)
    {
        if (this != &other)
        {
            unique_lock lock1(mutex_, defer_lock);
            shared_lock lock2(other.mutex_, defer_lock);
            lock(lock1, lock2);
            elements_ = other.elements_;
        }
        return *this;
    }

    inline void ElementList::push_back(Element element)
    {
        unique_lock lock(mutex_);
        elements_.push_back(std::move(element));
    }

    inline void ElementList::insert(size_t pos, Element element)
    {
        unique_lock lock(mutex_);
        if (pos <= elements_.size())
            elements_.insert(elements_.begin() + pos, std::move(element));
        else
            throw out_of_range("Insert position out of range");
    }

    inline const Element *ElementList::find(Identifier id) const
    {
        shared_lock lock(mutex_);
        for (const auto &e: elements_)
            if (e.id() == id)
                return &e;
        return nullptr;
    }

    inline Element *ElementList::find_mutable(Identifier id)
    {
        unique_lock lock(mutex_);
        for (auto &e: elements_)
            if (e.id() == id)
                return &e;
        return nullptr;
    }

    inline ElementList::const_iterator ElementList::begin() const
    {
        shared_lock lock(mutex_);
        return {elements_.begin(), this};
    }

    inline ElementList::const_iterator ElementList::end() const
    {
        shared_lock lock(mutex_);
        return {elements_.end(), this};
    }

    inline vector<Element> ElementList::snapshot() const
    {
        shared_lock lock(mutex_);
        return elements_;
    }

    inline size_t ElementList::size() const
    {
        shared_lock lock(mutex_);
        return elements_.size();
    }
    inline void ElementList::clear()
    {
        unique_lock lock(mutex_);
        elements_.clear();
    }

    inline ElementList::const_iterator &ElementList::const_iterator::operator++()
    {
        ++it_;
        return *this;
    }

    inline ElementList::const_iterator ElementList::const_iterator::operator++(int)
    {
        auto tmp = *this;
        ++it_;
        return tmp;
    }
    template<typename Predicate>
    inline void ElementList::erase_if(Predicate pred)
    {
        unique_lock lock(mutex_);
        std::erase_if(elements_, pred);
    }
} // namespace SF::EBML
