#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <ID/GUID.hpp>

namespace SF::Engine
{
    class Object
    {
    public:
        Object() : guid(GUID::Generate()) {}

        Object(const Object &other) : guid(GUID::Generate()) {}

        Object(Object &&other) noexcept : guid(std::move(other.guid)) {}

        Object &operator=(const Object &other)
        {
            return *this;
        }

        Object &operator=(Object &&other) noexcept
        {
            if (this != &other)
            {
                guid = std::move(other.guid);
            }
            return *this;
        }

        virtual ~Object() = default;

        const GUID &GetGUID() const { return guid; }

        std::string GetGUIDString() const { return guid.ToString(); }

        bool IsNull() const { return guid.IsNull(); }

        bool operator==(const Object &other) const { return guid == other.guid; }
        bool operator!=(const Object &other) const { return guid != other.guid; }
        bool operator<(const Object &other) const { return guid < other.guid; }

        struct GUIDHash
        {
            size_t operator()(const Object &obj) const
            {
                return GUID::Hash()(obj.guid);
            }
        };

    protected:
        GUID guid;
    };
}

#endif // OBJECT_HPP