#pragma once

#include <UtilityClasses/Patterns.hpp>
#include <memory>
#include <vector>
#include "ComputePipeline.hpp"

namespace SF::Engine
{
    /**
     * @brief Fluent construction settings for a ComputePipeline, mirroring
     * DescriptorSetWriteBuilder's style (RHI/Descriptors/DescriptorSetBuilder.hpp).
     *
     * ComputePipeline itself does all its real work in its constructor (shader load,
     * descriptor layout/pool, pipeline layout, PSO) rather than building up a small data
     * struct that's applied later the way a descriptor write is -- so this builder's "Build()"
     * step is Create(), which constructs the real ComputePipeline, rather than the base
     * BuilderPattern::Build() that just hands back the settings struct. Build() is still there
     * if you want the raw ComputePipelineDesc for some other purpose (serialization, diffing
     * against a previous config, etc).
     *
     * Usage:
     *   auto pipeline = ComputePipelineBuilder()
     *       .Shader("Shaders/Atmosphere/Atmosphere.shader")
     *       .LocalSize(8, 8, 1)
     *       .Create();
     */
    struct ComputePipelineDesc
    {
        std::filesystem::path shaderStage;
        std::string entry; // empty = use the shader's default entry point
        std::vector<Shader::Define> defines;
        bool pushDescriptors = false;
        std::vector<VkDescriptorSetLayout> additionalLayouts;
        UVec3 localSize{16, 16, 1};
    };

    class ComputePipelineBuilder : public BuilderPattern<ComputePipelineBuilder, ComputePipelineDesc>
    {
    public:
        ComputePipelineBuilder &Shader(std::filesystem::path shaderStage)
        {
            m_Product.shaderStage = std::move(shaderStage);
            return Self();
        }

        ComputePipelineBuilder &Entry(std::string entryPoint)
        {
            m_Product.entry = std::move(entryPoint);
            return Self();
        }

        ComputePipelineBuilder &Define(std::string name, std::string value = "")
        {
            m_Product.defines.emplace_back(std::move(name), std::move(value));
            return Self();
        }

        ComputePipelineBuilder &Defines(std::vector<Shader::Define> defines)
        {
            m_Product.defines = std::move(defines);
            return Self();
        }

        ComputePipelineBuilder &PushDescriptors(bool value = true)
        {
            m_Product.pushDescriptors = value;
            return Self();
        }

        ComputePipelineBuilder &AdditionalLayout(VkDescriptorSetLayout layout)
        {
            m_Product.additionalLayouts.push_back(layout);
            return Self();
        }

        ComputePipelineBuilder &AdditionalLayouts(std::vector<VkDescriptorSetLayout> layouts)
        {
            m_Product.additionalLayouts = std::move(layouts);
            return Self();
        }

        ComputePipelineBuilder &LocalSize(UVec3 size)
        {
            m_Product.localSize = size;
            return Self();
        }

        ComputePipelineBuilder &LocalSize(uint32_t x, uint32_t y, uint32_t z = 1)
        {
            m_Product.localSize = {x, y, z};
            return Self();
        }

        // Constructs the real ComputePipeline from the accumulated settings -- this is the
        // "Apply()" of this builder, matching DescriptorSetWrites::Apply()'s role of the step
        // that actually does the Vulkan work.
        [[nodiscard]] std::unique_ptr<ComputePipeline> Create() const
        {
            if (m_Product.entry.empty())
                return std::make_unique<ComputePipeline>(m_Product.shaderStage, m_Product.defines,
                                                         m_Product.pushDescriptors, m_Product.additionalLayouts,
                                                         m_Product.localSize);

            return std::make_unique<ComputePipeline>(m_Product.shaderStage, m_Product.entry, m_Product.defines,
                                                     m_Product.pushDescriptors, m_Product.additionalLayouts,
                                                     m_Product.localSize);
        }
    };
} // namespace SF::Engine
