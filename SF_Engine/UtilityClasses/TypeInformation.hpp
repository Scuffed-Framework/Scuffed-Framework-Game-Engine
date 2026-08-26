/******************************************************************************/
/* TypeInformation.hpp                                                        */
/******************************************************************************/
/*                            This file is part of                            */
/*                                SF Game Engine                              */
/******************************************************************************/
/* MIT License                                                                */
/*                                                                            */
/* Copyright (c) 2025-present Noah Lee                                        */
/*                                                                            */
/* May all those that this source may reach be blessed by the LORD and find   */
/* peace and joy in life.                                                     */
/* Everyone who drinks of this water will be thirsty again; but whoever       */
/* drinks of the water that I will give him shall never thirst; John 4:13-14  */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                        */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS    */
/* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                 */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY       */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT  */
/* OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE      */
/* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                              */
/******************************************************************************/

#pragma once

#include <typeindex>
#include <unordered_map>
#include <type_traits>
#include <string>
#include <shared_mutex>
#include <mutex>

#if defined(_MSC_VER)
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

namespace SF::Engine
{

    static std::string Demangle(const char *name)
    {
        char out[1024];
        if (UnDecorateSymbolName(name, out, sizeof(out), UNDNAME_COMPLETE))
            return out;
        return name;
    }
}
#elif defined(__GNUG__)
#include <cxxabi.h>
namespace SF::Engine
{
    static std::string Demangle(const char *name)
    {
        int status = 0;
        char *demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        std::string result = (status == 0 ? demangled : name);
        free(demangled);
        return result;
    }
}
#else

namespace SF::Engine
{
    static std::string Demangle(const char *name) { return name; }
}
#endif

namespace SF::Engine
{
    using TypeId = std::size_t;

    /**
     * @brief Thread-safe type information system
     * @tparam T Base type for the type hierarchy
     */
    template <typename T>
    class TypeInformation
    {
    public:
        TypeInformation() = delete;

        /**
         * @brief Get the type ID for a derived type K
         * @tparam K The derived type (must be convertible to T*)
         * @return Unique type ID for K within the T hierarchy
         */
        template <typename K>
            requires std::is_convertible_v<K *, T *>
        [[nodiscard]] static TypeId GetTypeId() noexcept
        {
            static_assert(std::is_convertible_v<K *, T *>,
                          "TypeInformation<K>: K must derive from T");
            // Use double-checked locking pattern for thread safety
            const std::type_index typeIndex(typeid(K));

            // First check without lock (read)
            {
                std::shared_lock lock(s_mutex);
                if (auto it = s_typeMap.find(typeIndex); it != s_typeMap.end())
                    return it->second;
            }

            // Not found, acquire write lock
            std::unique_lock lock(s_mutex);

            // Double-check after acquiring write lock
            if (auto it = s_typeMap.find(typeIndex); it != s_typeMap.end())
                return it->second;

            // Create new ID
            const auto id = s_nextTypeId++;
            s_typeMap[typeIndex] = id;
            return id;
        }

        /**
         * @brief Get the type name for a derived type K
         * @tparam K The derived type
         * @return Type name as string_view
         */
        template <typename K>
            requires std::is_convertible_v<K *, T *>
        [[nodiscard]] static std::string_view GetTypeName() noexcept
        {
            return Demangle(typeid(K).name());
        }

        /**
         * @brief Get the number of registered types
         */
        [[nodiscard]] static size_t GetRegisteredTypeCount() noexcept
        {
            std::shared_lock lock(s_mutex);
            return s_typeMap.size();
        }

        /**
         * @brief Check if a type is registered
         */
        template <typename K>
            requires std::is_convertible_v<K *, T *>
        [[nodiscard]] static bool IsRegistered() noexcept
        {
            std::shared_lock lock(s_mutex);
            const std::type_index typeIndex(typeid(K));
            return s_typeMap.find(typeIndex) != s_typeMap.end();
        }

        /**
         * @brief Clear all type registrations (use with caution!)
         */
        static void Clear() noexcept
        {
            std::unique_lock lock(s_mutex);
            s_typeMap.clear();
            s_nextTypeId = 0;
        }

    private:
        inline static TypeId s_nextTypeId = 0;
        inline static std::unordered_map<std::type_index, TypeId> s_typeMap = {};
        inline static std::shared_mutex s_mutex;
    };

    /**
     * @brief Alias for convenience
     */
    template <typename T>
    using TypeInfo = TypeInformation<T>;
}

#define Define_TypeId_Function(base, cl) [[nodiscard]] ::SF::Engine::TypeId GetTypeId() const override { return ::SF::Engine::TypeInformation<base>::GetTypeId<cl>(); }