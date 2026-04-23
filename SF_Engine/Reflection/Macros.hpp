#include <array>
#include <string_view>

#define ModuleEntryPoint void ModuleInit()

#define ExportClass(...)
#define NoExport(...) // Define members inside an FClass() as not exportable.
#define _API_MODULE_MAIN()

#define REFLECT(...)                               \
    static constexpr auto ReflectMembers()         \
    {                                              \
        return std::make_tuple(__VA_ARGS__);       \
    }                                              \
    static constexpr auto ReflectNames()           \
    {                                              \
        return std::array{                         \
            /* convert identifiers into strings */ \
            std::string_view{#__VA_ARGS__}};       \
    }

namespace SF::Engine
{
    constexpr auto SplitNames(std::string_view names)
    {
        std::array<std::string_view, 16> result{}; // max 16 members
        size_t count = 0;

        size_t start = 0;
        for (size_t i = 0; i <= names.size(); i++)
        {
            if (i == names.size() || names[i] == ',')
            {
                auto part = names.substr(start, i - start);
                // remove spaces
                while (!part.empty() && part.front() == ' ')
                    part.remove_prefix(1);
                result[count++] = part;
                start = i + 1;
            }
        }
        return std::pair(result, count);
    }
    template <typename T>
    void SerializeObject(XMLNode &parent, const std::string &name, const T &obj)
    {
        XMLNode node = parent.AddChild(name);

        constexpr auto members = T::ReflectMembers();
        constexpr auto rawNames = T::ReflectNames();
        constexpr auto [names, count] = SplitNames(rawNames[0]);

        std::apply([&](auto &&...member)
                   {
        size_t index = 0;
        ((Serialize(node, std::string(names[index++]), member)), ...); }, members);
    }
    template <typename T>
    T DeserializeObject(const XMLNode &parent, const std::string &name)
    {
        T obj;
        XMLNode node = parent.GetChild(name);
        if (!node.IsValid())
            return obj;

        constexpr auto members = T::ReflectMembers();
        constexpr auto rawNames = T::ReflectNames();
        constexpr auto [names, count] = SplitNames(rawNames[0]);

        std::apply([&](auto &&...member)
                   {
        size_t index = 0;
        ((member = Deserialize<std::decay_t<decltype(member)>>(
             node, std::string(names[index++]), member)), ...); }, members);

        return obj;
    }
};
