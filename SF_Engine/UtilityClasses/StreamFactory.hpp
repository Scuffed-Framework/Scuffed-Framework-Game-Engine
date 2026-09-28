#pragma once

#include <Engine/Log/Log.hpp>
#include <LowLevel/XML/XMLModule.hpp>
#include <UtilityClasses/TypeInformation.hpp>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace SF::Engine
{
    using namespace std;
    /**
     * @brief Factory for creating stream-based objects by name
     * @tparam Base Base class for all creatable types
     * @tparam Args Constructor arguments for created objects
     */
    template<typename Base, typename... Args>
    class StreamFactory
    {
        SF_RTTI_BASE(StreamFactory)
    public:
        using CreateReturn   = unique_ptr<Base>;
        using CreateFunction = function<CreateReturn(Args...)>;
        using RegistryMap    = unordered_map<string, CreateFunction>;

        virtual ~StreamFactory() = default;

        // For factories with arguments
        static CreateReturn Create(string_view name, Args... args)
            requires(sizeof...(Args) > 0)
        {
            const string nameStr(name);
            auto it = Registry().find(nameStr);

            if (it == Registry().end())
            {
                Log::Error("Failed to create '", name, "' - not found in factory registry");
                return nullptr;
            }

            return it->second(forward<Args>(args)...);
        }

        // For factories with no arguments
        static CreateReturn Create(string_view name)
            requires(sizeof...(Args) == 0)
        {
            const string nameStr(name);
            auto it = Registry().find(nameStr);

            if (it == Registry().end())
            {
                Log::Error("Failed to create '", name, "' - not found in factory registry");
                return nullptr;
            }

            return it->second();
        }

        static RegistryMap &Registry()
        {
            static RegistryMap impl;
            return impl;
        }

        static bool IsRegistered(string_view name)
        {
            const string nameStr(name);
            return Registry().contains(nameStr);
        }

        template<typename T>
        class Registrar : public Base
        {
        public:
            [[nodiscard]] TypeId GetTypeId() const override { return TypeInfo<Base>::template GetTypeId<T>(); }

            [[nodiscard]] string_view GetTypeName() const override { return s_name; }

            ~Registrar() = default;

        protected:
            static bool Register(string_view name)
            {
                s_name = name;

                if constexpr (sizeof...(Args) == 0)
                {
                    StreamFactory::Registry()[string(name)] = []() -> CreateReturn { return make_unique<T>(); };
                } else
                {
                    StreamFactory::Registry()[string(name)] = [](Args... args) -> CreateReturn
                    { return make_unique<T>(forward<Args>(args)...); };
                }

                return true;
            }

        private:
            inline static string_view s_name;
        };
    };

/**
 * @brief Helper macro for registering stream types
 * Usage: REGISTER_STREAM(MyStream, "MyStreamName")
 */
#define REGISTER_STREAM(StreamClass, StreamName)                                                                       \
    inline static bool StreamClass##_registered = StreamClass::Register(StreamName)

} // namespace SF::Engine
