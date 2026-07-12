#pragma once
#include <Reflection/RTTI/SerializeContext.hpp>
#include <XML/XMLReader.hpp>
#include <vector>
#include <cstdint>

namespace SF::Engine
{
    class XmlNodeWriter final : public SF::RTTI::IWriter
    {
    public:
        explicit XmlNodeWriter(XMLNode &root) { stack_.push_back(root); }

        void WriteFloat(const char *name, float v) override { stack_.back().SetAttribute(name, v); }
        void WriteUInt32(const char *name, std::uint32_t v) override { stack_.back().SetAttribute(name, static_cast<int>(v)); }
        void WriteInt32(const char *name, std::int32_t v) override { stack_.back().SetAttribute(name, static_cast<int>(v)); }
        void WriteBool(const char *name, bool v) override { stack_.back().SetAttribute(name, v); }
        void WriteString(const char *name, const std::string &v) override { stack_.back().SetAttribute(name, v); }

        void BeginObject(const char *name) override { stack_.push_back(stack_.back().AddChild(name)); }
        void EndObject() override { stack_.pop_back(); }

    private:
        std::vector<XMLNode> stack_;
    };

    class XmlNodeReader final : public SF::RTTI::IReader
    {
    public:
        explicit XmlNodeReader(const XMLNode &root) { stack_.push_back(root); }

        bool ReadFloat(const char *name, float &out) override { return stack_.back().GetAttribute(name, out); }
        bool ReadInt32(const char *name, std::int32_t &out) override
        {
            int raw{};
            if (!stack_.back().GetAttribute(name, raw))
            {
                return false;
            }
            out = raw;
            return true;
        }
        bool ReadUInt32(const char *name, std::uint32_t &out) override
        {
            int raw{};
            if (!stack_.back().GetAttribute(name, raw))
            {
                return false;
            }
            out = static_cast<std::uint32_t>(raw);
            return true;
        }
        bool ReadBool(const char *name, bool &out) override { return stack_.back().GetAttribute(name, out); }
        bool ReadString(const char *name, std::string &out) override { return stack_.back().GetAttribute(name, out); }

        bool BeginObject(const char *name) override
        {
            XMLNode child = stack_.back().GetChild(name);
            if (!child.IsValid())
            {
                return false;
            }
            stack_.push_back(child);
            return true;
        }
        void EndObject() override { stack_.pop_back(); }

    private:
        std::vector<XMLNode> stack_;
    };
}