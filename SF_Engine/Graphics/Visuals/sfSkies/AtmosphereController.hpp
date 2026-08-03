#pragma once
#include "Atmosphere/AtmospherePipelinePass.hpp"
#include <Controllers/Controller.hpp>
#include <Math/KVP.hpp>

#include <TemplateLibrary/Containers/AdvancedString.hpp>
#include <glm/glm.hpp>
#include <functional>

namespace SF::Engine
{
    struct AtmosphereEntry
    {
        ::SFTL::String name;
        AtmosphereData data;
        AtmospherePipelinePass *pass = nullptr; // non-owning; owned by PipelinePassManager
        glm::vec3 planetPos = {0.0f, 0.0f, 0.0f};
        bool active = true;
    };

    class AtmosphereController : public StaticController<AtmosphereController>
    {
    public:
        using PassFactory = std::function<AtmospherePipelinePass *(Pipeline::Stage, const AtmosphereParams &)>;

        explicit AtmosphereController(Pipeline::Stage stage, PassFactory factory)
            : stage_(stage), factory_(std::move(factory))
        {
        }

        explicit AtmosphereController(Pipeline::Stage stage, PassFactory factory,
                                       const ::SFTL::DynamicArray<KeyValuePair<::SFTL::String, AtmosphereData>> &params)
            : stage_(stage), factory_(std::move(factory))
        {
            entries_.reserve(params.size());
            for (const auto &[name, data] : params)
                entries_.emplace_back(AtmosphereEntry{ name, data, factory_(stage_, data.params), {0.0f, 0.0f, 0.0f}, true });
        }

        void Update(float DeltaTime) override
        {
        }

        void SetFrameData(const glm::mat4 &invProj, const glm::mat4 &invView,
                           const glm::vec3 &cameraPos, const glm::vec3 &sunDir,
                           const glm::vec2 &screenSize)
        {
            for (auto &entry : entries_)
            {
                if (!entry.active || !entry.pass)
                    continue;

                entry.pass->SetFrameData(invProj, invView, cameraPos, entry.planetPos, sunDir, screenSize);
            }
        }

        void SetActive(const ::SFTL::String &name, bool active)
        {
            if (AtmosphereEntry *entry = Find(name))
                entry->active = active;
        }

        void SetPlanetPosition(const ::SFTL::String &name, const glm::vec3 &pos)
        {
            if (AtmosphereEntry *entry = Find(name))
                entry->planetPos = pos;
        }

        void AddAtmosphere(const ::SFTL::String &name, const AtmosphereData &data, const glm::vec3 &planetPos = {})
        {
            entries_.emplace_back(AtmosphereEntry{ name, data, factory_(stage_, data.params), planetPos, true });
        }

        void RemoveAtmosphere(const ::SFTL::String &name)
        {
            auto newEnd = ::SFTL::remove_if(entries_.begin(), entries_.end(),
                [&name](const AtmosphereEntry &e) { return e.name == name; });
            entries_.erase(newEnd, entries_.end());
        }

        bool Empty() const { return entries_.empty(); }

    private:
        AtmosphereEntry *Find(const ::SFTL::String &name)
        {
            for (auto &entry : entries_)
                if (entry.name == name)
                    return &entry;
            return nullptr;
        }

        Pipeline::Stage stage_;
        PassFactory factory_;
        ::SFTL::DynamicArray<AtmosphereEntry> entries_;
    };
}