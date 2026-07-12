#pragma once
//
// SF Engine :: Reflection :: SerializeContext.h
//
// Usage (mirrors AZ::SerializeContext):
//
//   struct CloudscapeShaderConstantData
//   {
//       SF_TYPE_INFO(CloudscapeShaderConstantData)
//
//       float m_uvwScale = 1.0f;
//       uint32 m_maxMipLevels = 4;
//       ...
//
//       static void Reflect(sf::reflect::ReflectContext* context)
//       {
//           if (auto* serializeContext = ::rtti_cast<sf::reflect::SerializeContext>(context))
//           {
//               serializeContext->Class<CloudscapeShaderConstantData>()
//                   ->Version(1)
//                   ->Field("UVWScale", &CloudscapeShaderConstantData::m_uvwScale)
//                   ->Field("MaxMipLevels", &CloudscapeShaderConstantData::m_maxMipLevels);
//           }
//       }
//   };
//
// Fields are stored type-erased (one small heap alloc per field, at
// reflection time only -- never per-frame, never per-instance). Reading
// and writing goes through IWriter/IReader below; wire SerializeValue's
// primitive overloads to your own XML manifest read/write calls (or your
// existing ADL SaveAssetPayload/LoadAssetPayload customization points --
// see the note at the bottom of this file) if you'd rather not maintain
// a second serialization backend.
//
#include "ReflectContext.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace SF::RTTI
{
    template <typename T, typename = void>
    struct ReflectedTypeName
    {
    };

    template <typename T>
    struct ReflectedTypeName<T, std::enable_if_t<HasRtti<T>>>
    {
        static const char *Get() { return T::RTTI_TypeName(); }
    };

    template <typename T, typename = void>
    struct IsReflectable : std::false_type
    {
    };

    template <typename T>
    struct IsReflectable<T, std::void_t<decltype(ReflectedTypeName<T>::Get())>> : std::true_type
    {
    };

    class SerializeContext;

    class IWriter
    {
    public:
        virtual ~IWriter() = default;
        virtual void WriteFloat(const char *name, float v) = 0;
        virtual void WriteUInt32(const char *name, uint32 v) = 0;
        virtual void WriteInt32(const char *name, int32 v) = 0;
        virtual void WriteBool(const char *name, bool v) = 0;
        virtual void WriteString(const char *name, const std::string &v) = 0;
        virtual void BeginObject(const char *name) = 0;
        virtual void EndObject() = 0;
    };

    class IReader
    {
    public:
        virtual ~IReader() = default;
        virtual bool ReadFloat(const char *name, float &out) = 0;
        virtual bool ReadUInt32(const char *name, uint32 &out) = 0;
        virtual bool ReadInt32(const char *name, int32 &out) = 0;
        virtual bool ReadBool(const char *name, bool &out) = 0;
        virtual bool ReadString(const char *name, std::string &out) = 0;
        virtual bool BeginObject(const char *name) = 0;
        virtual void EndObject() = 0;
    };

    struct ClassData
    {
        TypeId typeId;
        const char *name = nullptr;
        uint32 version = 0;
        std::vector<std::unique_ptr<class IFieldBinding>> fields;
    };

    class IFieldBinding
    {
    public:
        virtual ~IFieldBinding() = default;
        virtual const char *GetName() const = 0;
        virtual TypeId GetFieldTypeId() const = 0;
        virtual void Save(const void *instance, IWriter &writer, const SerializeContext &context) const = 0;
        virtual void Load(void *instance, IReader &reader, const SerializeContext &context) const = 0;
    };

    template <typename FieldT>
    void SerializeValue(IWriter &writer, const char *name, const FieldT &value, const SerializeContext &context);

    template <typename FieldT>
    bool DeserializeValue(IReader &reader, const char *name, FieldT &value, const SerializeContext &context);

    template <typename ClassT, typename FieldT>
    class FieldBinding final : public IFieldBinding
    {
    public:
        FieldBinding(const char *name, FieldT ClassT::*member) : m_name(name), m_member(member) {}

        const char *GetName() const override { return m_name; }
        TypeId GetFieldTypeId() const override { return GetTypeId<FieldT>(); }

        void Save(const void *instance, IWriter &writer, const SerializeContext &context) const override
        {
            const ClassT *obj = static_cast<const ClassT *>(instance);
            SerializeValue<FieldT>(writer, m_name, obj->*m_member, context);
        }

        void Load(void *instance, IReader &reader, const SerializeContext &context) const override
        {
            ClassT *obj = static_cast<ClassT *>(instance);
            DeserializeValue<FieldT>(reader, m_name, obj->*m_member, context);
        }

    private:
        const char *m_name;
        FieldT ClassT::*m_member;
    };

    template <typename T>
    class ClassBuilder
    {
    public:
        ClassBuilder(ClassData &data) : m_data(data) {}

        ClassBuilder &Version(uint32 version)
        {
            m_data.version = version;
            return *this;
        }

        template <typename FieldT>
        ClassBuilder &Field(const char *name, FieldT T::*member)
        {
            m_data.fields.push_back(std::make_unique<FieldBinding<T, FieldT>>(name, member));
            return *this;
        }

        ClassBuilder *operator->() { return this; }

    private:
        ClassData &m_data;
    };

    class SerializeContext : public ReflectContext
    {
        SF_RTTI(SerializeContext, ReflectContext)
    public:
        static SerializeContext &Instance()
        {
            static SerializeContext instance;
            return instance;
        }

        template <typename T>
        ClassBuilder<T> Class()
        {
            static_assert(IsReflectable<T>::value,
                          "T needs SF_TYPE_INFO (or SF_RTTI/SF_RTTI_BASE) to be reflected, "
                          "or use SF_REFLECT_EXTERNAL_TYPE(T) for external/POD types.");
            TypeId id = GetTypeId<T>();
            ClassData &data = m_classes[id];
            data.typeId = id;
            data.name = ReflectedTypeName<T>::Get();
            return ClassBuilder<T>(data);
        }

        const ClassData *FindClassData(TypeId id) const
        {
            auto it = m_classes.find(id);
            return it != m_classes.end() ? &it->second : nullptr;
        }

        template <typename T>
        void Save(const T &instance, IWriter &writer) const
        {
            if (const ClassData *data = FindClassData(GetTypeId<T>()))
            {
                for (auto &field : data->fields)
                {
                    field->Save(&instance, writer, *this);
                }
            }
        }

        template <typename T>
        void Load(T &instance, IReader &reader) const
        {
            if (const ClassData *data = FindClassData(GetTypeId<T>()))
            {
                for (auto &field : data->fields)
                {
                    field->Load(&instance, reader, *this);
                }
            }
        }

    private:
        std::unordered_map<TypeId, ClassData> m_classes;
    };

    template <typename FieldT>
    void SerializeValue(IWriter &writer, const char *name, const FieldT &value, const SerializeContext &context)
    {
        if constexpr (is_same_v<FieldT, float>)
        {
            writer.WriteFloat(name, value);
        }
        else if constexpr (is_same_v<FieldT, uint32_t>)
        {
            writer.WriteUInt32(name, value);
        }
        else if constexpr (is_same_v<FieldT, int32_t>)
        {
            writer.WriteInt32(name, value);
        }
        else if constexpr (is_same_v<FieldT, bool>)
        {
            writer.WriteBool(name, value);
        }
        else if constexpr (is_same_v<FieldT, std::string>)
        {
            writer.WriteString(name, value);
        }
        else if constexpr (is_enum_v<FieldT>)
        {
            writer.WriteInt32(name, static_cast<int32>(value));
        }
        else
        {
            writer.BeginObject(name);
            if (const ClassData *nested = context.FindClassData(GetTypeId<FieldT>()))
            {
                for (auto &field : nested->fields)
                {
                    field->Save(&value, writer, context);
                }
            }
            writer.EndObject();
        }
    }

    template <typename FieldT>
    bool DeserializeValue(IReader &reader, const char *name, FieldT &value, const SerializeContext &context)
    {
        if constexpr (is_same_v<FieldT, float>)
        {
            return reader.ReadFloat(name, value);
        }
        else if constexpr (is_same_v<FieldT, uint32>)
        {
            return reader.ReadUInt32(name, value);
        }
        else if constexpr (is_same_v<FieldT, int32>)
        {
            return reader.ReadInt32(name, value);
        }
        else if constexpr (is_same_v<FieldT, bool>)
        {
            return reader.ReadBool(name, value);
        }
        else if constexpr (is_same_v<FieldT, std::string>)
        {
            return reader.ReadString(name, value);
        }
        else if constexpr (is_enum_v<FieldT>)
        {
            int32 raw{};
            if (!reader.ReadInt32(name, raw))
            {
                return false;
            }
            value = static_cast<FieldT>(raw);
            return true;
        }
        else
        {
            if (!reader.BeginObject(name))
            {
                return false;
            }
            if (const ClassData *nested = context.FindClassData(GetTypeId<FieldT>()))
            {
                for (auto &field : nested->fields)
                {
                    field->Load(&value, reader, context);
                }
            }
            reader.EndObject();
            return true;
        }
    }

}

#define SF_REFLECT_EXTERNAL_TYPE(TypeName)             \
    template <>                                        \
    struct SF::RTTI::ReflectedTypeName<TypeName>       \
    {                                                  \
        static const char *Get() { return #TypeName; } \
    };