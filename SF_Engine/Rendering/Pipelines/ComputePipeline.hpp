#pragma once

#include <Rendering/Commands/CommandBuffer.hpp>
#include <Rendering/Shaders/Shader.hpp>
#include <Math/Vectors/Vector.hpp>
#include "Pipeline.hpp"
#include <filesystem>
#include <memory>

namespace SF::Engine
{
    /**
     * @brief Class that represents a compute pipeline.
     */
    class ComputePipeline : public Pipeline
    {
    public:
        /**
         * Creates a new compute pipeline.
         * @param shaderStage The shader file that will be loaded.
         * @param defines     A list of preprocessor defines (uses Shader::Define, same as RenderPipeline).
         * @param pushDescriptors If no actual descriptor sets are allocated but instead pushed.
         * @param additionalLayouts Extra descriptor set layouts appended after set 0 (this
         *        pipeline's own set) and set 1 (shared samplers) — e.g. a bindless or
         *        per-dispatch set.
         * @param localSize The workgroup size (local_size_x/y/z) this shader was authored
         *        with. Used by the 2-arg CmdRender(extent) overload to compute dispatch
         *        group counts — must match the shader's actual `local_size_*` layout qualifiers.
         *        Must be non-zero in every component.
         */
        explicit ComputePipeline(
            std::filesystem::path shaderStage,
            std::vector<Shader::Define> defines = {},
            bool pushDescriptors = false,
            std::vector<VkDescriptorSetLayout> additionalLayouts = {},
            UVec3 localSize = {16, 16, 1});

        /**
         * Creates a new compute pipeline.
         * @param shaderStage The shader file that will be loaded.
         * @param entry     the entry point to use
         * @param defines     A list of preprocessor defines (uses Shader::Define, same as RenderPipeline).
         * @param pushDescriptors If no actual descriptor sets are allocated but instead pushed.
         * @param additionalLayouts See other constructor.
         * @param localSize See other constructor.
         */
        explicit ComputePipeline(
            std::filesystem::path shaderStage,
            std::string entry,
            std::vector<Shader::Define> defines = {},
            bool pushDescriptors = false,
            std::vector<VkDescriptorSetLayout> additionalLayouts = {},
            UVec3 localSize = {16, 16, 1});

        ~ComputePipeline() override;

        /// Dispatches ceil(extent / localSize) groups in x/y, 1 group in z.
        /// Uses the localSize this pipeline was constructed with.
        void CmdRender(const CommandBuffer &commandBuffer, const UVec2 &extent) const;

        /// Dispatches ceil(extent.x/X) * ceil(extent.y/Y) groups in x/y, and exactly
        /// `zGroups` groups in z — e.g. for cubemap faces, cascades, or array layers
        /// that aren't part of `extent`.
        void CmdRender(const CommandBuffer &commandBuffer, const UVec2 &extent,
                       const uint32_t X, const uint32_t Y, const uint32_t zGroups) const;

        /// Dispatches ceil(extent / {LOCAL_X, LOCAL_Y, LOCAL_Z}) groups in all 3 dimensions.
        void CmdRender(const CommandBuffer &commandBuffer, const UVec3 &extent,
                       const uint32_t LOCAL_X, const uint32_t LOCAL_Y, const uint32_t LOCAL_Z) const;

        [[nodiscard]] const std::filesystem::path &GetShaderStage() const { return shaderStage; }
        [[nodiscard]] const std::string &GetEntry() const { return entryOpt; }
        [[nodiscard]] const std::vector<Shader::Define> &GetDefines() const { return defines; }
        [[nodiscard]] const UVec3 &GetLocalSize() const { return localSize; }
        [[nodiscard]] bool IsPushDescriptors() const override { return pushDescriptors; }
        [[nodiscard]] const Shader *GetShader() const override { return shader.get(); }
        [[nodiscard]] const VkDescriptorSetLayout &GetDescriptorSetLayout() const override { return descriptorSetLayout; }
        [[nodiscard]] const VkDescriptorPool &GetDescriptorPool() const override { return descriptorPool; }
        [[nodiscard]] const VkPipeline &GetPipeline() const override { return pipeline; }
        [[nodiscard]] const VkPipelineLayout &GetPipelineLayout() const override { return pipelineLayout; }
        [[nodiscard]] const VkPipelineBindPoint &GetPipelineBindPoint() const override { return pipelineBindPoint; }

        void ReloadShader(const std::vector<uint32_t> &newSpirv);

    private:
        void CreateShaderProgram(std::string entry);
        void CreateDescriptorLayout();
        void CreateDescriptorPool();
        void CreatePipelineLayout();
        void CreatePipelineCompute();

        std::filesystem::path shaderStage;
        std::vector<Shader::Define> defines;
        bool pushDescriptors;
        std::vector<VkDescriptorSetLayout> additionalLayouts;
        UVec3 localSize;

        std::shared_ptr<Shader> shader;
        std::string entryOpt;

        VkDevice device_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    };
}