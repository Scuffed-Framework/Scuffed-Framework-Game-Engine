#pragma once

#include <Graphics/Commands/CommandBuffer.hpp>
#include <Graphics/Shaders/Shader.hpp>
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
         */
        explicit ComputePipeline(
            std::filesystem::path shaderStage,
            std::vector<Shader::Define> defines = {},
            bool pushDescriptors = false);

        ~ComputePipeline();

        void CmdRender(const CommandBuffer &commandBuffer, const UVec2 &extent) const;
        void CmdRender(const CommandBuffer &commandBuffer, const UVec2 &extent, const uint32_t X, const uint32_t Y, const uint32_t Z) const;
        void CmdRender(const CommandBuffer &commandBuffer, const UVec3 &extent, const uint32_t LOCAL_X, const uint32_t LOCAL_Y, const uint32_t LOCAL_Z) const;

        const std::filesystem::path &GetShaderStage() const { return shaderStage; }
        const std::vector<Shader::Define> &GetDefines() const { return defines; }
        bool IsPushDescriptors() const override { return pushDescriptors; }
        const Shader *GetShader() const override { return shader.get(); }
        const VkDescriptorSetLayout &GetDescriptorSetLayout() const override { return descriptorSetLayout; }
        const VkDescriptorPool &GetDescriptorPool() const override { return descriptorPool; }
        const VkPipeline &GetPipeline() const override { return pipeline; }
        const VkPipelineLayout &GetPipelineLayout() const override { return pipelineLayout; }
        const VkPipelineBindPoint &GetPipelineBindPoint() const override { return pipelineBindPoint; }

        void ReloadShader(const std::vector<uint32_t> &newSpirv);

    private:
        void CreateShaderProgram();
        void CreateDescriptorLayout();
        void CreateDescriptorPool();
        void CreatePipelineLayout();
        void CreatePipelineCompute();

        std::filesystem::path shaderStage;
        std::vector<Shader::Define> defines;
        bool pushDescriptors;

        std::shared_ptr<Shader> shader;

        VkDevice device_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint pipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    };
}
