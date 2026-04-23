#pragma once
#include <XML/XMLReader.hpp>
#include <glm/glm.hpp>

namespace SF::Engine
{
    // Helper free functions to keep Serialize/Deserialize clean
    inline void SerializeVec3(XMLNode &node, const std::string &name, const glm::vec3 &v)
    {
        XMLNode child = node.AddChild(name);
        child.SetAttribute("x", v.x);
        child.SetAttribute("y", v.y);
        child.SetAttribute("z", v.z);
    }

    inline glm::vec3 DeserializeVec3(const XMLNode &node, const std::string &name, glm::vec3 defaultVal = {})
    {
        XMLNode child = node.GetChild(name);
        if (!child.IsValid())
            return defaultVal;
        glm::vec3 out = defaultVal;
        child.GetAttribute("x", out.x);
        child.GetAttribute("y", out.y);
        child.GetAttribute("z", out.z);
        return out;
    }
    inline void SerializeVec4(XMLNode &node, const std::string &name, const glm::vec4 &v)
    {
        XMLNode child = node.AddChild(name);
        child.SetAttribute("x", v.x);
        child.SetAttribute("y", v.y);
        child.SetAttribute("z", v.z);
        child.SetAttribute("w", v.w);
    }

    inline glm::vec4 DeserializeVec4(const XMLNode &node, const std::string &name, glm::vec4 defaultVal = {})
    {
        XMLNode child = node.GetChild(name);
        if (!child.IsValid())
            return defaultVal;
        glm::vec4 out = defaultVal;
        child.GetAttribute("x", out.x);
        child.GetAttribute("y", out.y);
        child.GetAttribute("z", out.z);
        child.GetAttribute("w", out.w);
        return out;
    }

    inline void SerializeFloat(XMLNode &node, const std::string &name, const float &f)
    {
        node.SetAttribute(name, f);
    }

    inline float DeserializeFloat(const XMLNode &node, const std::string &name, float defaultVal = 0.0f)
    {
        float out = defaultVal;
        node.GetAttribute(name, out);
        return out;
    }

    template <typename T>
    void Serialize(XMLNode &node, const std::string &name, const T &value)
    {
        static_assert(sizeof(T) == 0, "Serialize<T> has no specialization for this type");
    }

    template <typename T>
    T Deserialize(const XMLNode &node, const std::string &name, const T &defaultVal = {})
    {
        static_assert(sizeof(T) == 0, "Deserialize<T> has no specialization for this type");
        return defaultVal;
    }
}