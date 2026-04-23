#pragma once

#include <memory>
#include <typeindex>
#include <unordered_map>

#include <UtilityClasses/ThreadPool.hpp>
#include "Engine/Engine.hpp"
#include "Resource.hpp"

namespace SF::Engine
{
    /**
     * @brief Module used for managing resources. Resources are held alive as long as they are in
     * use, a existing resource is queried by node value.
     */
    class Resources : public ModuleRegistrar<Resources>
    {
        // Register the module
        const bool Registered =
            ModuleFactory<Module>::RegisterModule<Resources>(Module::Stage::Post);

    public:
        Resources();

        void Update() override;

        std::shared_ptr<Resource> Find(const std::type_index& typeIndex, char* name) const;

        template <typename T>
        std::shared_ptr<T> Find(const char* data) const;

        void Add(const std::shared_ptr<Resource>& resource, char* name);
        void Remove(const std::shared_ptr<Resource>& resource);

        /**
         * Gets the resource loader thread pool.
         * @return The resource loader thread pool.
         */
        ThreadPool& GetThreadPool()
        {
            return threadPool;
        }

    private:
        // Map from type_index to map of names to resources
        std::unordered_map<std::type_index,
                           std::unordered_map<std::string, std::shared_ptr<Resource>>>
            resources;

        SF::Engine::ElapsedTime elapsedPurge;

        ThreadPool threadPool;
    };
}