#pragma once

#include "Identifier.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <algorithm>

namespace SF::EBML
{
    using byte = std::uint8_t;

    // Forward declaration
    class Element;

    // Thread-safe element list with copy-on-write semantics
    class ElementList
    {
    private:
        mutable std::shared_mutex mutex_;
        std::vector<Element> elements_;

    public:
        // NOTE: Element and ElementList are mutually referential (Element holds
        // an ElementList of children; ElementList holds a vector<Element>).
        // Any method here that needs to construct/copy/inspect an Element must
        // be defined out-of-line, after Element's definition later in this
        // file - Element is only forward-declared at this point. Methods that
        // don't touch Element itself (size/empty/clear, and the erase_if
        // template, whose instantiation is deferred until use) can stay inline.
        ElementList();
        ElementList(const ElementList& other);
        ElementList(ElementList&& other) noexcept;
        ElementList& operator=(const ElementList& other);

        void push_back(Element element);
        void insert(size_t pos, Element element);

        template<typename Predicate>
        void erase_if(Predicate pred)
        {
            std::unique_lock lock(mutex_);
            elements_.erase(std::remove_if(elements_.begin(), elements_.end(), pred),
                           elements_.end());
        }

        size_t size() const
        {
            std::shared_lock lock(mutex_);
            return elements_.size();
        }

        bool empty() const
        {
            std::shared_lock lock(mutex_);
            return elements_.empty();
        }

        // Find with shared lock
        const Element* find(Identifier id) const;

        // Non-const find with unique lock
        Element* find_mutable(Identifier id);

        // Iterator support with locking
        class const_iterator
        {
        private:
            typename std::vector<Element>::const_iterator it_;
            const ElementList* parent_;
        public:
            const_iterator(typename std::vector<Element>::const_iterator it, const ElementList* parent)
                : it_(it), parent_(parent) {}
            
            const Element& operator*() const { return *it_; }
            const Element* operator->() const { return &*it_; }
            
            const_iterator& operator++() { ++it_; return *this; }
            const_iterator operator++(int) { auto tmp = *this; ++it_; return tmp; }
            
            bool operator==(const const_iterator& other) const { return it_ == other.it_; }
            bool operator!=(const const_iterator& other) const { return it_ != other.it_; }
        };

        const_iterator begin() const;
        const_iterator end() const;

        // Range-based access with lock (returns snapshot)
        std::vector<Element> snapshot() const;

        // Get mutable reference with lock (for serialization)
        const std::vector<Element>& get_unsafe() const { return elements_; }
        
        // Clear all elements
        void clear()
        {
            std::unique_lock lock(mutex_);
            elements_.clear();
        }
    };

    // Decode functions
    inline std::uint64_t decode_uint(std::span<const byte> data)
    {
        if (data.size() > 8)
            throw std::runtime_error("EBML integer element must be at most 8 bytes");
        std::uint64_t v = 0;
        for (byte b : data)
            v = (v << 8) | b;
        return v;
    }

    inline std::int64_t decode_int(std::span<const byte> data)
    {
        std::uint64_t v = decode_uint(data);
        if (!data.empty() && (data[0] & 0x80) && data.size() < 8)
            v |= (~std::uint64_t{0}) << (data.size() * 8);
        return static_cast<std::int64_t>(v);
    }

    inline double decode_float(std::span<const byte> data)
    {
        if (data.size() == 4)
        {
            std::uint32_t bits = static_cast<std::uint32_t>(decode_uint(data));
            float f;
            std::memcpy(&f, &bits, 4);
            return f;
        }
        if (data.size() == 8)
        {
            std::uint64_t bits = decode_uint(data);
            double d;
            std::memcpy(&d, &bits, 8);
            return d;
        }
        if (data.empty())
            return 0.0;
        throw std::runtime_error("EBML float element must be 0, 4 or 8 bytes");
    }

    inline std::vector<byte> encode_uint(std::uint64_t v)
    {
        std::vector<byte> out;
        do
        {
            out.insert(out.begin(), static_cast<byte>(v & 0xFF));
            v >>= 8;
        } while (v);
        return out;
    }

    inline std::vector<byte> encode_int(std::int64_t v)
    {
        std::vector<byte> out;
        std::uint64_t u = static_cast<std::uint64_t>(v);
        bool negative = v < 0;
        while (true)
        {
            out.insert(out.begin(), static_cast<byte>(u & 0xFF));
            bool leadingSignOk = negative ? (out.front() & 0x80) != 0
                                          : (out.front() & 0x80) == 0;
            std::uint64_t next = negative ? (u >> 8) | (~std::uint64_t{0} << 56)
                                          : (u >> 8);
            if (leadingSignOk && (negative ? next == (~std::uint64_t{0}) : next == 0))
                break;
            u = next;
        }
        return out;
    }

    inline std::vector<byte> encode_float(double v)
    {
        std::uint64_t bits;
        std::memcpy(&bits, &v, 8);
        auto out = encode_uint(bits);
        while (out.size() < 8)
            out.insert(out.begin(), 0);
        return out;
    }

    inline std::vector<byte> encode_float32(float v)
    {
        std::uint32_t bits;
        std::memcpy(&bits, &v, 4);
        auto out = encode_uint(bits);
        while (out.size() < 4)
            out.insert(out.begin(), 0);
        return out;
    }

    inline constexpr std::int64_t kDateEpochOffsetNs = 978307200LL * 1'000'000'000LL;

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

        static constexpr std::string_view kind_name(Kind k)
        {
            switch (k)
            {
                case Kind::Master: return "Master";
                case Kind::UInt: return "UInt";
                case Kind::Int: return "Int";
                case Kind::Float: return "Float";
                case Kind::String: return "String";
                case Kind::Utf8: return "Utf8";
                case Kind::Date: return "Date";
                case Kind::Binary: return "Binary";
                default: return "Unknown";
            }
        }

    private:
        Identifier m_id;
        Kind m_kind = Kind::Binary;
        mutable std::shared_mutex mutex_;
        std::variant<ElementList, std::vector<byte>> m_payload;

    public:
        Element() = default;
        
        Element(Identifier id, Kind kind, ElementList children)
            : m_id(id), m_kind(kind), m_payload(std::move(children)) {}
        
        Element(Identifier id, Kind kind, std::vector<byte> data)
            : m_id(id), m_kind(kind), m_payload(std::move(data)) {}

        // Copy constructor (thread-safe)
        Element(const Element& other)
            : m_id(other.m_id), m_kind(other.m_kind)
        {
            std::shared_lock lock(other.mutex_);
            m_payload = other.m_payload;
        }

        // Move constructor
        Element(Element&& other) noexcept
            : m_id(other.m_id), m_kind(other.m_kind), m_payload(std::move(other.m_payload)) {}

        // Assignment operators
        Element& operator=(const Element& other)
        {
            if (this != &other)
            {
                std::unique_lock lock1(mutex_, std::defer_lock);
                std::shared_lock lock2(other.mutex_, std::defer_lock);
                std::lock(lock1, lock2);
                m_id = other.m_id;
                m_kind = other.m_kind;
                m_payload = other.m_payload;
            }
            return *this;
        }

        Element& operator=(Element&& other) noexcept
        {
            if (this != &other)
            {
                m_id = other.m_id;
                m_kind = other.m_kind;
                m_payload = std::move(other.m_payload);
            }
            return *this;
        }

        // Accessors
        Identifier id() const { return m_id; }
        Kind kind() const { return m_kind; }
        bool is_master() const { return m_kind == Kind::Master; }

        // Children access with locking
        ElementList& children() 
        { 
            return std::get<ElementList>(m_payload); 
        }
        
        const ElementList& children() const 
        { 
            return std::get<ElementList>(m_payload); 
        }
        
        void add(Element child) 
        { 
            children().push_back(std::move(child)); 
        }

        const Element* find(Identifier childId) const 
        { 
            return children().find(childId); 
        }
        
        Element* find_mutable(Identifier childId) 
        { 
            return children().find_mutable(childId); 
        }

        // Raw data access
        std::vector<byte>& raw() 
        { 
            return std::get<std::vector<byte>>(m_payload); 
        }
        
        const std::vector<byte>& raw() const 
        { 
            return std::get<std::vector<byte>>(m_payload); 
        }

        // Type-safe accessors with validation - FIXED: using static decode functions
        std::uint64_t as_uint() const
        {
            if (m_kind != Kind::UInt)
                throw std::runtime_error("Element is not a UInt");
            return decode_uint(raw());
        }

        std::int64_t as_int() const
        {
            if (m_kind != Kind::Int)
                throw std::runtime_error("Element is not an Int");
            return decode_int(raw());
        }

        double as_float() const
        {
            if (m_kind != Kind::Float)
                throw std::runtime_error("Element is not a Float");
            return decode_float(raw());
        }

        std::string as_string() const
        {
            if (m_kind != Kind::String && m_kind != Kind::Utf8)
                throw std::runtime_error("Element is not a String or Utf8");
            return {raw().begin(), raw().end()};
        }

        std::chrono::sys_time<std::chrono::nanoseconds> as_date() const
        {
            if (m_kind != Kind::Date)
                throw std::runtime_error("Element is not a Date");
            std::int64_t ns = decode_int(raw());
            return std::chrono::sys_time<std::chrono::nanoseconds>{
                std::chrono::nanoseconds{ns + kDateEpochOffsetNs}};
        }

        // Factory methods
        static Element make_master(Identifier id)
        {
            return Element(id, Kind::Master, ElementList{});
        }

        static Element make_uint(Identifier id, std::uint64_t v)
        {
            return Element(id, Kind::UInt, encode_uint(v));
        }

        static Element make_int(Identifier id, std::int64_t v)
        {
            return Element(id, Kind::Int, encode_int(v));
        }

        static Element make_float(Identifier id, double v)
        {
            return Element(id, Kind::Float, encode_float(v));
        }

        static Element make_float32(Identifier id, float v)
        {
            return Element(id, Kind::Float, encode_float32(v));
        }

        static Element make_date(Identifier id, std::chrono::sys_time<std::chrono::nanoseconds> tp)
        {
            std::int64_t ns = tp.time_since_epoch().count() - kDateEpochOffsetNs;
            return Element(id, Kind::Date, encode_int(ns));
        }

        static Element make_string(Identifier id, std::string_view v)
        {
            return Element(id, Kind::String, std::vector<byte>(v.begin(), v.end()));
        }

        static Element make_utf8(Identifier id, std::string_view v)
        {
            return Element(id, Kind::Utf8, std::vector<byte>(v.begin(), v.end()));
        }

        static Element make_binary(Identifier id, std::vector<byte> v)
        {
            return Element(id, Kind::Binary, std::move(v));
        }

        static Element make_leaf(Identifier id, Kind kind, std::vector<byte> data)
        {
            return Element(id, kind, std::move(data));
        }

        // Validation
        bool validate() const
        {
            if (m_kind == Kind::Master)
            {
                for (const auto& child : children())
                    if (!child.validate())
                        return false;
            }
            else if (m_kind == Kind::UInt || m_kind == Kind::Int)
            {
                if (raw().empty() || raw().size() > 8)
                    return false;
            }
            else if (m_kind == Kind::Float)
            {
                if (raw().size() != 4 && raw().size() != 8)
                    return false;
            }
            else if (m_kind == Kind::Date)
            {
                if (raw().empty() || raw().size() > 8)
                    return false;
            }
            return true;
        }
    };

    // --- Out-of-line ElementList members that require Element to be complete ---

    inline ElementList::ElementList() = default;

    inline ElementList::ElementList(const ElementList& other)
    {
        std::shared_lock lock(other.mutex_);
        elements_ = other.elements_;
    }

    inline ElementList::ElementList(ElementList&& other) noexcept
    {
        std::unique_lock lock(other.mutex_);
        elements_ = std::move(other.elements_);
    }

    inline ElementList& ElementList::operator=(const ElementList& other)
    {
        if (this != &other)
        {
            std::unique_lock lock1(mutex_, std::defer_lock);
            std::shared_lock lock2(other.mutex_, std::defer_lock);
            std::lock(lock1, lock2);
            elements_ = other.elements_;
        }
        return *this;
    }

    inline void ElementList::push_back(Element element)
    {
        std::unique_lock lock(mutex_);
        elements_.push_back(std::move(element));
    }

    inline void ElementList::insert(size_t pos, Element element)
    {
        std::unique_lock lock(mutex_);
        if (pos <= elements_.size())
            elements_.insert(elements_.begin() + pos, std::move(element));
        else
            throw std::out_of_range("Insert position out of range");
    }

    inline const Element* ElementList::find(Identifier id) const
    {
        std::shared_lock lock(mutex_);
        for (const auto& e : elements_)
            if (e.id() == id)
                return &e;
        return nullptr;
    }

    inline Element* ElementList::find_mutable(Identifier id)
    {
        std::unique_lock lock(mutex_);
        for (auto& e : elements_)
            if (e.id() == id)
                return &e;
        return nullptr;
    }

    inline ElementList::const_iterator ElementList::begin() const
    {
        std::shared_lock lock(mutex_);
        return const_iterator(elements_.begin(), this);
    }

    inline ElementList::const_iterator ElementList::end() const
    {
        std::shared_lock lock(mutex_);
        return const_iterator(elements_.end(), this);
    }

    inline std::vector<Element> ElementList::snapshot() const
    {
        std::shared_lock lock(mutex_);
        return elements_;
    }
}