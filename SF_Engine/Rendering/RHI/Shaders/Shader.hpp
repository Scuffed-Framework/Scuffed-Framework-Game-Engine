#pragma once

#ifndef VK_NO_PROTOTYPES
    #define VK_NO_PROTOTYPES
#endif

#include <Math/BasicMath.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <spirv_reflect.h>

#include <volk.h>

#ifdef None
    #undef None
#endif

namespace SF::Engine
{
    using namespace std;
    struct UniformInfo
    {
        uint32_t binding;
        uint32_t offset;
        uint32_t size;
        VkDescriptorType type;
        VkShaderStageFlags stageFlags;
        bool readOnly  = false;
        bool writeOnly = false;

        bool operator==(const UniformInfo &other) const
        {
            return binding == other.binding && offset == other.offset && size == other.size && type == other.type &&
                   stageFlags == other.stageFlags && readOnly == other.readOnly && writeOnly == other.writeOnly;
        }
        auto operator<=>(const UniformInfo &) const = default;
    };
    struct PushConstantRange
    {
        uint32_t offset;
        uint32_t size;
        VkShaderStageFlags stageFlags;
    };

    class Shader
    {
    public:
        /**
         * @brief Represents a vertex input binding and attributes
         */
        class VertexInput
        {
        public:
            VertexInput(vector<VkVertexInputBindingDescription> bindings     = {},
                        vector<VkVertexInputAttributeDescription> attributes = {}) :
                bindingDescriptions(std::move(bindings)), attributeDescriptions(std::move(attributes))
            {
            }

            virtual ~VertexInput() = default;

            [[nodiscard]] const vector<VkVertexInputBindingDescription> &GetBindingDescriptions() const
            {
                return bindingDescriptions;
            }

            [[nodiscard]] const vector<VkVertexInputAttributeDescription> &GetAttributeDescriptions() const
            {
                return attributeDescriptions;
            }

            bool operator<(const VertexInput &other) const
            {
                if (bindingDescriptions.empty() || other.bindingDescriptions.empty())
                    return false;
                return bindingDescriptions.front().binding < other.bindingDescriptions.front().binding;
            }

        protected:
            vector<VkVertexInputBindingDescription> bindingDescriptions;
            vector<VkVertexInputAttributeDescription> attributeDescriptions;
        };

        /**
         * @brief Represents a preprocessor define for shaders
         */
        struct Define
        {
            string name;
            string value;

            Define(string n = "", string v = "") : name(std::move(n)), value(std::move(v)) {}

            bool operator<(const Define &other) const { return name < other.name; }

            bool operator==(const Define &other) const { return name == other.name && value == other.value; }
        };

        /**
         * @brief Represents a uniform block (UBO/SSBO/Push Constants)
         */
        class UniformBlock
        {
        public:
            enum class Type
            {
                None,
                Uniform,
                Storage,
                Push
            };

            explicit UniformBlock(int32_t binding = -1, int32_t size = -1, VkShaderStageFlags stageFlags = 0,
                                  Type type = Type::Uniform) :
                binding(binding), size(size), stageFlags(stageFlags), type(type)
            {
            }

            [[nodiscard]] int32_t GetBinding() const { return binding; }
            [[nodiscard]] int32_t GetSize() const { return size; }
            [[nodiscard]] VkShaderStageFlags GetStageFlags() const { return stageFlags; }
            [[nodiscard]] Type GetType() const { return type; }
            [[nodiscard]] const map<string, UniformInfo> &GetUniforms() const { return uniforms; }

            [[nodiscard]] optional<UniformInfo> GetUniform(const string &name) const
            {
                auto it = uniforms.find(name);
                if (it == uniforms.end())
                    return nullopt;
                return it->second;
            }

            bool operator==(const UniformBlock &rhs) const
            {
                return binding == rhs.binding && size == rhs.size && stageFlags == rhs.stageFlags && type == rhs.type &&
                       uniforms == rhs.uniforms;
            }

            bool operator!=(const UniformBlock &rhs) const { return !operator==(rhs); }

        private:
            friend class Shader;
            int32_t binding;
            int32_t size;
            VkShaderStageFlags stageFlags;
            Type type;
            map<string, UniformInfo> uniforms;
        };

        /**
         * @brief Represents a vertex attribute
         */
        class Attribute
        {
        public:
            explicit Attribute(int32_t set = -1, int32_t location = -1, int32_t size = -1,
                               VkFormat format = VK_FORMAT_UNDEFINED) :
                set(set), location(location), size(size), format(format)
            {
            }

            [[nodiscard]] int32_t GetSet() const { return set; }
            [[nodiscard]] int32_t GetLocation() const { return location; }
            [[nodiscard]] int32_t GetSize() const { return size; }
            [[nodiscard]] VkFormat GetFormat() const { return format; }

            bool operator==(const Attribute &rhs) const
            {
                return set == rhs.set && location == rhs.location && size == rhs.size && format == rhs.format;
            }

        private:
            int32_t set;
            int32_t location;
            int32_t size;
            VkFormat format;
        };

        /**
         * @brief One compiled stage of a graphics pipeline.
         */
        struct StageSpirv
        {
            VkShaderStageFlagBits stage;
            vector<uint32_t> spirv;
        };

        // Factory methods

        /**
         * Generic graphics factory: builds a Shader from whichever stages are supplied
         * (vertex, tess control, tess eval, geometry, fragment). Stages are stored in
         * pipeline order regardless of input order. Returns nullptr if a stage appears
         * twice, a SPIR-V blob is empty, or a shader module could not be created.
         * This is the extension point for mesh/task stages later.
         */
        static shared_ptr<Shader> CreateFromStages(VkDevice device, const vector<StageSpirv> &stages);

        /// Convenience wrapper over CreateFromStages (empty tess/geometry blobs are skipped).
        static shared_ptr<Shader> CreateFromSPIRV(VkDevice device, const vector<uint32_t> &vertSpirv,
                                                  const vector<uint32_t> &fragSpirv,
                                                  const vector<uint32_t> &tessCtrlSpirv = {},
                                                  const vector<uint32_t> &tessEvalSpirv = {},
                                                  const vector<uint32_t> &geomSpirv     = {});

        static shared_ptr<Shader> CreateComputeFromSPIRV(VkDevice device, const vector<uint32_t> &computeSpirv);

        static shared_ptr<Shader> CreateFromFile(VkDevice device, const filesystem::path &vertPath,
                                                 const filesystem::path &fragPath);

        ~Shader();

        // Pipeline creation helpers
        vector<VkPipelineShaderStageCreateInfo> GetPipelineStages() const;
        VkDescriptorSetLayout CreateDescriptorSetLayout() const;
        vector<VkPushConstantRange> GetPushConstantRanges() const;

        // Reflection data accessors
        const unordered_map<string, UniformInfo> &GetUniforms() const { return uniforms; }
        const map<string, UniformBlock> &GetUniformBlocks() const { return uniformBlocks; }
        const map<string, Attribute> &GetAttributes() const { return attributes; }
        const vector<PushConstantRange> &GetPushConstants() const { return pushConstants; }
        const vector<VkDescriptorSetLayoutBinding> &GetDescriptorBindings() const { return descriptorBindings; }
        const vector<VkDescriptorPoolSize> &GetDescriptorPools() const { return descriptorPools; }
        const vector<VkVertexInputAttributeDescription> &GetAttributeDescriptions() const
        {
            return attributeDescriptions;
        }

        // Query methods
        optional<uint32_t> GetDescriptorLocation(const string &name) const;
        optional<uint32_t> GetDescriptorSize(const string &name) const;
        optional<UniformInfo> GetUniform(const string &name) const;
        optional<UniformBlock> GetUniformBlock(const string &name) const;
        optional<Attribute> GetAttribute(const string &name) const;
        optional<VkDescriptorType> GetDescriptorType(uint32_t location) const;

        bool HasStage(VkShaderStageFlagBits stage) const;
        uint32_t GetLastDescriptorBinding() const { return lastDescriptorBinding; }

        // Hot reload support
        void Reload(const vector<uint32_t> &newSpirv, VkShaderStageFlagBits stage);

        // Utility
        static VkShaderStageFlagBits GetShaderStageFromExtension(const string &filepath);
        static VkFormat GlTypeToVk(int32_t type);

    private:
        Shader(VkDevice device);

        void ReflectFromSPIRV(const vector<uint32_t> &spirv, VkShaderStageFlagBits stage);
        void AddShaderModule(const vector<uint32_t> &spirv, VkShaderStageFlagBits stage);
        void LoadUniformBlock(const SpvReflectBlockVariable &block, VkShaderStageFlagBits stage);
        void IncrementDescriptorPool(map<VkDescriptorType, uint32_t> &poolCounts, VkDescriptorType type);
        void Cleanup();

        VkDevice device;

        struct ModuleInfo
        {
            VkShaderModule module;
            VkShaderStageFlagBits stage;
        };

        vector<ModuleInfo> moduleInfos;
        unordered_map<string, UniformInfo> uniforms;
        map<string, UniformBlock> uniformBlocks;
        map<string, Attribute> attributes;
        vector<PushConstantRange> pushConstants;
        vector<VkDescriptorSetLayoutBinding> descriptorBindings;
        vector<VkDescriptorPoolSize> descriptorPools;
        vector<VkVertexInputAttributeDescription> attributeDescriptions;

        map<string, uint32_t> descriptorLocations;
        map<string, uint32_t> descriptorSizes;
        map<uint32_t, VkDescriptorType> descriptorTypes;
        uint32_t lastDescriptorBinding = 0;

        mutable vector<string> notFoundNames;
    };

    // Helper function implementations
    inline VkShaderStageFlagBits Shader::GetShaderStageFromExtension(const string &filepath)
    {
        if (filepath.ends_with(".vert") || filepath.ends_with(".vs"))
            return VK_SHADER_STAGE_VERTEX_BIT;
        if (filepath.ends_with(".frag") || filepath.ends_with(".fs") || filepath.ends_with(".ps"))
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        if (filepath.ends_with(".comp") || filepath.ends_with(".cs"))
            return VK_SHADER_STAGE_COMPUTE_BIT;
        if (filepath.ends_with(".geom") || filepath.ends_with(".gs"))
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        if (filepath.ends_with(".tesc") || filepath.ends_with(".hs"))
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (filepath.ends_with(".tese") || filepath.ends_with(".ds"))
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        return VK_SHADER_STAGE_ALL;
    }
} // namespace SF::Engine
