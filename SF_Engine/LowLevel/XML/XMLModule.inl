#pragma once

namespace SF::Engine
{
    template <typename T>
    bool XMLModule::Serialize(const std::string &name, const T &object, const std::string &filename)
    {
        static_assert(std::is_base_of_v<Serializable, T>,
                      "T must inherit from Serializable");
        SetRootNode(name);
        XMLNode root = GetRootNode();
        object.Serialize(root);
        return SaveToFile(filename);
    }

    template <typename T>
    bool XMLModule::Deserialize(const std::string &filename, T &object)
    {
        static_assert(std::is_base_of_v<Serializable, T>,
                      "T must inherit from Serializable");
        if (!LoadFromFile(filename))
            return false;
        XMLNode root = GetRootNode();
        object.Deserialize(root);
        return true;
    }
}