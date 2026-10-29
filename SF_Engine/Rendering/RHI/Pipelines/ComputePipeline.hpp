#pragma once

#include <Math/Vectors/Vector.hpp>
#include <Rendering/RHI/Commands/CommandBuffer.hpp>
#include <Rendering/RHI/Shaders/Shader.hpp>
#include <filesystem>
#include <memory>
#include "Pipeline.hpp"

namespace SF::Engine
{
    using namespace std;
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
         *        pipeline's own set) and set 1 (shared samplers); e.g. a bindless or
         *        per-dispatch set.
         * @param localSize The workgroup size (local_size_x/y/z) this shader was authored
         *        with. Used by the 2-arg CmdRender(extent) overload to compute dispatch
         *        group counts; must match the shader's actual `local_size_*` layout qualifiers.
         *        Must be non-zero in every component.
         */
        explicit ComputePipeline(filesystem::path shaderStage, vector<Shader::Define> defines = {},
                                 bool pushDescriptors = false, vector<VkDescriptorSetLayout> additionalLayouts = {},
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
        explicit ComputePipeline(filesystem::path shaderStage, const string &entry, vector<Shader::Define> defines = {},
                                 bool pushDescriptors = false, vector<VkDescriptorSetLayout> additionalLayouts = {},
                                 UVec3 localSize = {16, 16, 1});

        ~ComputePipeline() override;

        /// Dispatches ceil(extent / {LOCAL_X, LOCAL_Y, LOCAL_Z}) groups in all 3 dimensions.
        void Dispatch(const CommandBuffer &commandBuffer, const UVec3 &extent, const UVec3 &localSize = {}) const;

        [[nodiscard]] const filesystem::path &GetShaderStage() const { return shaderStage; }
        [[nodiscard]] const string &GetEntry() const { return entryOpt; }
        [[nodiscard]] const vector<Shader::Define> &GetDefines() const { return defines; }
        [[nodiscard]] const UVec3 &GetLocalSize() const { return localSize; }
        [[nodiscard]] bool IsPushDescriptors() const override { return pushDescriptors; }
        [[nodiscard]] const Shader *GetShader() const override { return shader.get(); }
        [[nodiscard]] const VkDescriptorSetLayout &GetDescriptorSetLayout() const override
        {
            return descriptorSetLayout;
        }
        [[nodiscard]] const VkDescriptorPool &GetDescriptorPool() const override { return descriptorPool; }
        [[nodiscard]] const VkPipeline &GetPipeline() const override { return pipeline; }
        [[nodiscard]] const VkPipelineLayout &GetPipelineLayout() const override { return pipelineLayout; }
        [[nodiscard]] const VkPipelineBindPoint &GetPipelineBindPoint() const override { return pipelineBindPoint; }

        void ReloadShader(const vector<uint32_t> &newSpirv) const;

    private:
        void CreateShaderProgram(const string &entry);
        void CreateDescriptorLayout();
        void CreateDescriptorPool();
        void CreatePipelineLayout();
        void CreatePipelineCompute();

        filesystem::path shaderStage;
        vector<Shader::Define> defines;
        bool pushDescriptors;
        vector<VkDescriptorSetLayout> additionalLayouts;
        UVec3 localSize;

        shared_ptr<Shader> shader;
        string entryOpt;

        VkDevice device_                          = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool           = VK_NULL_HANDLE;
        VkPipeline pipeline                       = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout           = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint     = VK_PIPELINE_BIND_POINT_COMPUTE;
    };
} // namespace SF::Engine
