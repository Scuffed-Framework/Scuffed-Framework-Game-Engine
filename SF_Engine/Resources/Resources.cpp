#include "Resources.hpp"

namespace SF::Engine
{
    Resources::Resources() : elapsedPurge(5s)
    {
    }

    void Resources::Update()
    {
        if (elapsedPurge.GetElapsed() != 0)
        {
            for (auto it = resources.begin(); it != resources.end();)
            {
                auto &typeMap = it->second;

                for (auto it1 = typeMap.begin(); it1 != typeMap.end();)
                {
                    if (it1->second.use_count() <= 1)
                    {
                        it1 = typeMap.erase(it1);
                        continue;
                    }

                    ++it1;
                }

                if (typeMap.empty())
                {
                    it = resources.erase(it);
                    continue;
                }

                ++it;
            }
        }
    }

    std::shared_ptr<Resource> Resources::Find(const std::type_index &typeIndex, char *name) const
    {
        auto typeIt = resources.find(typeIndex);
        if (typeIt == resources.end())
            return nullptr;

        const auto &typeMap = typeIt->second;
        auto resourceIt = typeMap.find(name);
        if (resourceIt == typeMap.end())
            return nullptr;

        return resourceIt->second;
    }

    template <typename T>
    std::shared_ptr<T> Resources::Find(const char *data) const
    {
        auto resource = Find(typeid(T), const_cast<char *>(data));
        if (!resource)
            return nullptr;

        return std::dynamic_pointer_cast<T>(resource);
    }

    void Resources::Add(const std::shared_ptr<Resource> &resource, char *name)
    {
        if (Find(resource->GetTypeIndex(), name))
            return;

        resources[resource->GetTypeIndex()].emplace(name, resource);
    }

    void Resources::Remove(const std::shared_ptr<Resource> &resource)
    {
        auto typeIt = resources.find(resource->GetTypeIndex());
        if (typeIt == resources.end())
            return;

        auto &typeMap = typeIt->second;
        for (auto it = typeMap.begin(); it != typeMap.end(); ++it)
        {
            if (it->second == resource)
            {
                typeMap.erase(it);
                break;
            }
        }

        if (typeMap.empty())
            resources.erase(resource->GetTypeIndex());
    }
}