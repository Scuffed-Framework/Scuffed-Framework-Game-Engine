#pragma once

#define VK_NO_PROTOTYPES

#include <filesystem>
#include <Math/BasicMath.hpp>
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
    struct UniformInfo
    {
        uint32_t binding;
        uint32_t offset;
        uint32_t size;
        VkDescriptorType type;
        VkShaderStageFlags stageFlags;
        bool readOnly = false;
        bool writeOnly = false;

        bool operator==(const UniformInfo &other) const
        {
            return binding == other.binding && offset == other.offset && size == other.size &&
                   type == other.type && stageFlags == other.stageFlags &&
                   readOnly == other.readOnly && writeOnly == other.writeOnly;
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
            VertexInput(std::vector<VkVertexInputBindingDescription> bindings = {},
                        std::vector<VkVertexInputAttributeDescription> attributes = {})
                : bindingDescriptions(std::move(bindings)),
                  attributeDescriptions(std::move(attributes))
            {
            }

            virtual ~VertexInput() = default;

            const std::vector<VkVertexInputBindingDescription> &GetBindingDescriptions() const
            {
                return bindingDescriptions;
            }

            const std::vector<VkVertexInputAttributeDescription> &GetAttributeDescriptions() const
            {
                return attributeDescriptions;
            }

            bool operator<(const VertexInput &other) const
            {
                if (bindingDescriptions.empty() || other.bindingDescriptions.empty())
                    return false;
                return bindingDescriptions.front().binding <
                       other.bindingDescriptions.front().binding;
            }

        protected:
            std::vector<VkVertexInputBindingDescription> bindingDescriptions;
            std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        };

        /**
         * @brief Represents a preprocessor define for shaders
         */
        struct Define
        {
            std::string name;
            std::string value;

            Define(std::string n = "", std::string v = "") : name(std::move(n)), value(std::move(v))
            {
            }

            bool operator<(const Define &other) const
            {
                return name < other.name;
            }

            bool operator==(const Define &other) const
            {
                return name == other.name && value == other.value;
            }
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

            explicit UniformBlock(int32_t binding = -1, int32_t size = -1,
                                  VkShaderStageFlags stageFlags = 0, Type type = Type::Uniform)
                : binding(binding), size(size), stageFlags(stageFlags), type(type)
            {
            }

            int32_t GetBinding() const
            {
                return binding;
            }
            int32_t GetSize() const
            {
                return size;
            }
            VkShaderStageFlags GetStageFlags() const
            {
                return stageFlags;
            }
            Type GetType() const
            {
                return type;
            }
            const std::map<std::string, UniformInfo> &GetUniforms() const
            {
                return uniforms;
            }

            std::optional<UniformInfo> GetUniform(const std::string &name) const
            {
                auto it = uniforms.find(name);
                if (it == uniforms.end())
                    return std::nullopt;
                return it->second;
            }

            bool operator==(const UniformBlock &rhs) const
            {
                return binding == rhs.binding && size == rhs.size && stageFlags == rhs.stageFlags &&
                       type == rhs.type && uniforms == rhs.uniforms;
            }

            bool operator!=(const UniformBlock &rhs) const
            {
                return !operator==(rhs);
            }

        private:
            friend class Shader;
            int32_t binding;
            int32_t size;
            VkShaderStageFlags stageFlags;
            Type type;
            std::map<std::string, UniformInfo> uniforms;
        };

        /**
         * @brief Represents a vertex attribute
         */
        class Attribute
        {
        public:
            explicit Attribute(int32_t set = -1, int32_t location = -1, int32_t size = -1,
                               VkFormat format = VK_FORMAT_UNDEFINED)
                : set(set), location(location), size(size), format(format)
            {
            }

            int32_t GetSet() const
            {
                return set;
            }
            int32_t GetLocation() const
            {
                return location;
            }
            int32_t GetSize() const
            {
                return size;
            }
            VkFormat GetFormat() const
            {
                return format;
            }

            bool operator==(const Attribute &rhs) const
            {
                return set == rhs.set && location == rhs.location && size == rhs.size &&
                       format == rhs.format;
            }

        private:
            int32_t set;
            int32_t location;
            int32_t size;
            VkFormat format;
        };

        // Factory methods
        static std::shared_ptr<Shader> CreateFromSPIRV(VkDevice device,
                                                       const std::vector<uint32_t> &vertSpirv,
                                                       const std::vector<uint32_t> &fragSpirv,
                                                       const std::vector<uint32_t> &tessCtrlSpirv = {},
                                                       const std::vector<uint32_t> &tessEvalSpirv = {});

        static std::shared_ptr<Shader> CreateComputeFromSPIRV(
            VkDevice device, const std::vector<uint32_t> &computeSpirv);

        static std::shared_ptr<Shader> CreateFromFile(VkDevice device,
                                                      const std::filesystem::path &vertPath,
                                                      const std::filesystem::path &fragPath);

        ~Shader();

        // Pipeline creation helpers
        std::vector<VkPipelineShaderStageCreateInfo> GetPipelineStages() const;
        VkDescriptorSetLayout CreateDescriptorSetLayout() const;
        std::vector<VkPushConstantRange> GetPushConstantRanges() const;

        // Reflection data accessors
        const std::unordered_map<std::string, UniformInfo> &GetUniforms() const
        {
            return uniforms;
        }
        const std::map<std::string, UniformBlock> &GetUniformBlocks() const
        {
            return uniformBlocks;
        }
        const std::map<std::string, Attribute> &GetAttributes() const
        {
            return attributes;
        }
        const std::vector<PushConstantRange> &GetPushConstants() const
        {
            return pushConstants;
        }
        const std::vector<VkDescriptorSetLayoutBinding> &GetDescriptorBindings() const
        {
            return descriptorBindings;
        }
        const std::vector<VkDescriptorPoolSize> &GetDescriptorPools() const
        {
            return descriptorPools;
        }
        const std::vector<VkVertexInputAttributeDescription> &GetAttributeDescriptions() const
        {
            return attributeDescriptions;
        }

        // Query methods
        std::optional<uint32_t> GetDescriptorLocation(const std::string &name) const;
        std::optional<uint32_t> GetDescriptorSize(const std::string &name) const;
        std::optional<UniformInfo> GetUniform(const std::string &name) const;
        std::optional<UniformBlock> GetUniformBlock(const std::string &name) const;
        std::optional<Attribute> GetAttribute(const std::string &name) const;
        std::optional<VkDescriptorType> GetDescriptorType(uint32_t location) const;

        bool HasStage(VkShaderStageFlagBits stage) const;
        uint32_t GetLastDescriptorBinding() const
        {
            return lastDescriptorBinding;
        }

        // Hot reload support
        void Reload(const std::vector<uint32_t> &newSpirv, VkShaderStageFlagBits stage);

        // Utility
        static VkShaderStageFlagBits GetShaderStageFromExtension(const std::string &filepath);
        static VkFormat GlTypeToVk(int32_t type);

    private:
        Shader(VkDevice device);

        void ReflectFromSPIRV(const std::vector<uint32_t> &spirv, VkShaderStageFlagBits stage);
        void AddShaderModule(const std::vector<uint32_t> &spirv, VkShaderStageFlagBits stage);
        void LoadUniformBlock(const SpvReflectBlockVariable &block, VkShaderStageFlagBits stage);
        void IncrementDescriptorPool(std::map<VkDescriptorType, uint32_t> &poolCounts,
                                     VkDescriptorType type);
        void Cleanup();

        VkDevice device;

        struct ModuleInfo
        {
            VkShaderModule module;
            VkShaderStageFlagBits stage;
        };

        std::vector<ModuleInfo> moduleInfos;
        std::unordered_map<std::string, UniformInfo> uniforms;
        std::map<std::string, UniformBlock> uniformBlocks;
        std::map<std::string, Attribute> attributes;
        std::vector<PushConstantRange> pushConstants;
        std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
        std::vector<VkDescriptorPoolSize> descriptorPools;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

        std::map<std::string, uint32_t> descriptorLocations;
        std::map<std::string, uint32_t> descriptorSizes;
        std::map<uint32_t, VkDescriptorType> descriptorTypes;
        uint32_t lastDescriptorBinding = 0;

        mutable std::vector<std::string> notFoundNames;
    };

    // Helper function implementations
    inline VkShaderStageFlagBits Shader::GetShaderStageFromExtension(const std::string &filepath)
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
}