#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <Engine/Log/Log.hpp>
#include <UtilityClasses/TypeInformation.hpp>

namespace SF::Engine
{
    /**
     * @brief Factory for creating stream-based objects by name
     * @tparam Base Base class for all creatable types
     * @tparam Args Constructor arguments for created objects
     */
    template <typename Base, typename... Args>
    class StreamFactory
    {
    public:
        using CreateReturn = std::unique_ptr<Base>;
        using CreateFunction = std::function<CreateReturn(Args...)>;
        using RegistryMap = std::unordered_map<std::string, CreateFunction>;

        virtual ~StreamFactory() = default;

        // For factories with arguments
        static CreateReturn Create(std::string_view name, Args... args)
            requires(sizeof...(Args) > 0)
        {
            const std::string nameStr(name);
            auto it = Registry().find(nameStr);

            if (it == Registry().end())
            {
                Log::Error("Failed to create '", name, "' - not found in factory registry");
                return nullptr;
            }

            return it->second(std::forward<Args>(args)...);
        }

        // For factories with no arguments
        static CreateReturn Create(std::string_view name)
            requires(sizeof...(Args) == 0)
        {
            const std::string nameStr(name);
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

        static bool IsRegistered(std::string_view name)
        {
            const std::string nameStr(name);
            return Registry().find(nameStr) != Registry().end();
        }

        template <typename T>
        class Registrar : public Base
        {
        public:
            TypeId GetTypeId() const override
            {
                return TypeInfo<Base>::template GetTypeId<T>();
            }

            std::string_view GetTypeName() const override
            {
                return s_name;
            }

        protected:
            static bool Register(std::string_view name)
            {
                s_name = name;

                if constexpr (sizeof...(Args) == 0)
                {
                    StreamFactory::Registry()[std::string(name)] = []() -> CreateReturn
                    {
                        return std::make_unique<T>();
                    };
                }
                else
                {
                    StreamFactory::Registry()[std::string(name)] = [](Args... args) -> CreateReturn
                    {
                        return std::make_unique<T>(std::forward<Args>(args)...);
                    };
                }

                return true;
            }

        private:
            inline static std::string_view s_name;
        };
    };

/**
 * @brief Helper macro for registering stream types
 * Usage: REGISTER_STREAM(MyStream, "MyStreamName")
 */
#define REGISTER_STREAM(StreamClass, StreamName)  \
    inline static bool StreamClass##_registered = \
        StreamClass::Register(StreamName)

} // namespace SF::Engine