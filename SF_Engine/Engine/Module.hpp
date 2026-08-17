#pragma once

#include <bitset>
#include <concepts>
#include <functional>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <UtilityClasses/NoCopy.hpp>
#include <UtilityClasses/TypeInformation.hpp>

#ifdef Always
#undef Always
#endif

namespace SF::Engine
{
    // Forward declaration
    class Module;

    /**
     * @brief Module update stages - defined before ModuleFactory to avoid circular dependency
     */
    enum class ModuleStage : uint8_t
    {
        Never,  // Module is never updated (utility module)
        Always, // Module is always updated (critical systems)
        Pre,    // Early update (input, events)
        Normal, // Standard update (game logic)
        Post,   // Late update (physics, cleanup)
        Render  // Rendering stage
    };

    enum class ModuleStartStage : uint8_t
    {
        ManualStartup, // default
        OnEngineInit,
        Normal,
        PostMainWindowInit
    };

    /**
     * @brief Concept to ensure a type is derived from Module
     */
    template <typename T>
    concept ModuleDerived = std::is_base_of_v<Module, T> && !std::is_same_v<Module, T>;

    /**
     * @brief Factory for creating and managing module instances
     */
    template <typename Base>
    class ModuleFactory
    {
    public:
        /**
         * @brief Creation information for a module
         */
        struct CreateInfo
        {
            std::function<std::unique_ptr<Base>()> createFunc;
            ModuleStage stage;
            std::vector<TypeId> dependencies;
            std::string_view name; // For debugging and logging
        };

        using RegistryMap = std::unordered_map<TypeId, CreateInfo>;

        virtual ~ModuleFactory() = default;

        /**
         * @brief Get the global module registry
         */
        static RegistryMap &Registry()
        {
            static RegistryMap impl;
            return impl;
        }

        /**
         * @brief Helper for specifying module dependencies
         */
        template <ModuleDerived... Args>
        class Requires // Ensure this is in a public section
        {
        public:
            std::vector<TypeId> Get() const
            {
                std::vector<TypeId> dependencies;
                dependencies.reserve(sizeof...(Args));
                (dependencies.emplace_back(TypeInfo<Base>::template GetTypeId<Args>()), ...);
                return dependencies;
            }
        };

        /**
         * @brief Base registrar class for modules
         */
        template <typename T>
        class Registrar : public Base
        {
        public: // Change this from protected to public
            virtual ~Registrar()
            {
                if (static_cast<T *>(this) == s_instance)
                    s_instance = nullptr;
            }

            static T *Get() noexcept
            {
                return s_instance;
            }
            static bool Exists() noexcept
            {
                return s_instance != nullptr;
            }

            // Move Register into the public section so derived classes can call it via the macro
            template <typename... Args>
            static bool Register(ModuleStage stage, Requires<Args...> dependencies = {})
            {
                s_registeredStage = stage;
                s_registeredName = typeid(T).name();

                ModuleFactory::Registry()[TypeInfo<Base>::template GetTypeId<T>()] = {
                    []() -> std::unique_ptr<Base>
                    {
                        s_instance = new T();
                        return std::unique_ptr<Base>(s_instance);
                    },
                    stage, dependencies.Get(), s_registeredName};

                return true;
            }

            inline static ModuleStage s_registeredStage = ModuleStage::Never;
            inline static std::string_view s_registeredName = "";

        private:
            inline static T *s_instance = nullptr;
        };

        template <typename T, typename... Args>
        static bool RegisterModule(ModuleStage stage, Requires<Args...> deps = {})
        {
            return Registrar<T>::Register(stage, deps);
        }

        template <typename T, typename... Args>
        struct AutoRegister
        {
            AutoRegister(ModuleStage stage, Requires<Args...> deps = {})
            {
                Registrar<T>::Register(stage, deps);
            }
        };
    };

    /**
     * @brief Base class for all engine modules
     */
    class Module : public ModuleFactory<Module>, NoCopy
    {
    public:
        /**
         * @brief Module update stages (alias to ModuleStage)
         */
        using Stage = ModuleStage;

        /**
         * @brief Stage and type identifier pair
         */
        using StageIndex = std::pair<Stage, TypeId>;

        virtual ~Module() = default;

        /**
         * @brief Update function called by the engine
         */
        virtual void Update() = 0;

        /**
         * @brief Optional initialization function
         * @return true if initialization succeeded, false otherwise
         */
        virtual bool Initialize()
        {
            return true;
        }

        /**
         * @brief Optional cleanup function
         */
        virtual void Shutdown() {}

        /**
         * @brief Get the module's update stage
         */
        virtual Stage GetStage() const = 0;

        /**
         * @brief Get the module's type ID
         */
        virtual TypeId GetTypeId() const = 0;

        /**
         * @brief Get the module's name (for debugging)
         */
        virtual std::string_view GetName() const = 0;
    };

    // Explicit template instantiation
    template class TypeInformation<Module>;

    /**
     * @brief Helper base class that implements Module's pure virtual methods using CRTP
     * All modules should inherit from ModuleRegistrar<YourModule> instead of
     * ModuleRegistrar<YourModule>
     */
    template <typename T>
    class ModuleRegistrar : public Module::Registrar<T>
    {
    public:
        // Implement the pure virtual methods from Module
        // FIX: Changed 'Stage' to 'ModuleStage' (or you could use 'Module::Stage')
        ModuleStage GetStage() const override
        {
            return ModuleRegistrar<T>::s_registeredStage;
        }

        TypeId GetTypeId() const override
        {
            return TypeInfo<Module>::template GetTypeId<T>();
        }

        std::string_view GetName() const override
        {
            return ModuleRegistrar<T>::s_registeredName;
        }
    };

    /**
     * @brief Filter for selectively including/excluding modules
     */
    class ModuleFilter
    {
    public:
        static constexpr size_t MaxModules = 128;

        ModuleFilter()
        {
            IncludeAll();
        }

        template <ModuleDerived T>
        [[nodiscard]] bool Check() const noexcept
        {
            const auto id = TypeInfo<Module>::GetTypeId<T>();
            return id < MaxModules && m_include.test(id);
        }

        [[nodiscard]] bool Check(TypeId typeId) const noexcept
        {
            return typeId < MaxModules && m_include.test(typeId);
        }

        template <ModuleDerived T>
        ModuleFilter &Exclude() noexcept
        {
            const auto id = TypeInfo<Module>::GetTypeId<T>();
            if (id < MaxModules)
                m_include.reset(id);
            return *this;
        }

        template <ModuleDerived T>
        ModuleFilter &Include() noexcept
        {
            const auto id = TypeInfo<Module>::GetTypeId<T>();
            if (id < MaxModules)
                m_include.set(id);
            return *this;
        }

        template <ModuleDerived... Args>
        ModuleFilter &Exclude() noexcept
        {
            (Exclude<Args>(), ...);
            return *this;
        }

        template <ModuleDerived... Args>
        ModuleFilter &Include() noexcept
        {
            (Include<Args>(), ...);
            return *this;
        }

        ModuleFilter &ExcludeAll() noexcept
        {
            m_include.reset();
            return *this;
        }

        ModuleFilter &IncludeAll() noexcept
        {
            m_include.set();
            return *this;
        }

        [[nodiscard]] size_t Count() const noexcept
        {
            return m_include.count();
        }

        [[nodiscard]] bool Any() const noexcept
        {
            return m_include.any();
        }

        [[nodiscard]] bool All() const noexcept
        {
            return m_include.all();
        }

    private:
        std::bitset<MaxModules> m_include;
    };

/**
 * @brief Helper macro for registering modules
 * Usage: REGISTER_MODULE(MyModule, ModuleStage::Normal, Module::Requires<Dep1, Dep2>{})
 */
#define REGISTER_MODULE(ModuleClass, UpdateStage, ...) \
    inline static bool ModuleClass##_registered = ModuleClass::Register(UpdateStage, ##__VA_ARGS__)

} // namespace SF::Engine