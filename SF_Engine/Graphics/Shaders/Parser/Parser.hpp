#pragma once

#define VK_NO_PROTOTYPES

#include <algorithm>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include "../Shader.hpp"

namespace SF::Engine::Shaders
{

    // Public enums
    enum class ShaderLanguage
    {
        GLSL,
        HLSL
    };

    enum class ShaderStage
    {
        Vertex,
        Fragment,
        Compute,
        Geometry,
        TessellationControl,
        TessellationEvaluation
    };

    // Property types for the Properties { } block
    enum class PropType
    {
        Int,
        Float,
        Bool,
        String,
        Color,
        Vector,
        // New extended types -- all stored in stringProps for backward compat
        Texture2D,
        Texture3D,
        TextureCube,
        Range,    // constrained float: Range(min, max)
        Enum,     // integer backed by a named C++ enum: Enum(TypeName)
        Gradient, // color ramp: list of (t, color) stops
    };

    struct ShaderProp
    {
        std::string name;
        std::string displayName; // optional human-readable label
        PropType type;
        std::string rawDefault; // unparsed default value, always preserved
        // --- Extended metadata (new, optional) ---
        std::string group;     // @group("...") annotation
        std::string tooltip;   // @tooltip("...") annotation
        std::string ifKeyword; // @if(KEYWORD) annotation
        // Range subtype bounds (valid when type == Range)
        float rangeMin = 0.0f;
        float rangeMax = 1.0f;
        // Enum type name (valid when type == Enum)
        std::string enumTypeName;
    };

    // Capability requirement (from #pragma require)
    struct ShaderCapabilityRequirement
    {
        std::string feature; // e.g. "compute_shaders", "texture3d"
        int tier = 0;        // optional tier (0 = any)
    };

    // Specialization constant (from #pragma specialize)
    struct ShaderSpecializationConstant
    {
        std::string name;
        std::string defaultValue;
        uint32_t constantId = 0; // assigned in declaration order
    };

    // Resource binding entry (from ResourceLayout { } block)
    struct ResourceBinding
    {
        uint32_t set = 0;
        uint32_t binding = 0;
        std::string resourceType; // "sampler2D", "sampler3D", "image2D", "uniformbuf", etc.
        std::string name;
        std::string qualifiers; // e.g. "writeonly rgba16f"
    };

    // Per-pass render state (from Pass { } blocks)
    struct PassRenderState
    {
        // Cull mode: "Off", "Front", "Back" (default "Back")
        std::string cullMode;
        // ZWrite: "On" / "Off"
        std::string zWrite;
        // Blend: src dst factors (e.g. "One OneMinusSrcAlpha")
        std::string blendSrc;
        std::string blendDst;
        // Tags: arbitrary key→value pairs
        std::map<std::string, std::string> tags;
    };

    // SubShader LOD
    struct SubShaderDesc
    {
        int lod = 0;
        std::map<std::string, std::string> tags;
    };

    // Public structs -- everything callers might need
    struct ComputeKernel
    {
        std::string name;
        std::string entryPoint;
        glm::uvec3 workgroupSize = {};
        bool hasWorkgroupSize = false;
    };

    struct ParsedShaderStage
    {
        ShaderStage stage;
        std::string source;
        std::string entryPoint = "main";
        std::vector<ComputeKernel> kernels;
        // Workgroup size extracted from layout(local_size_*) or #pragma workgroup_size
        glm::uvec3 workgroupSize = {1, 1, 1};
        bool hasWorkgroupSize = false;
        // Pass membership (empty string = top-level / legacy stage)
        std::string passName;
        // Render state from enclosing Pass { } block
        PassRenderState renderState;
    };

    struct ShaderIncludeEntry
    {
        std::string path;
        std::vector<ShaderStage> stages;
        bool isImport = true;
    };

    struct ParsedShader
    {
        std::string name;
        std::string filepath;
        ShaderLanguage language = ShaderLanguage::GLSL;

        std::vector<ParsedShaderStage> stages;
        std::vector<ShaderIncludeEntry> includes;

        // --- Typed convenience maps (populated from Properties block) ---
        // Backward-compatible: always populated regardless of new features used.
        std::map<std::string, std::string> stringProps;
        std::map<std::string, int> intProps;
        std::map<std::string, float> floatProps;
        std::map<std::string, bool> boolProps;

        // Full ordered property list with metadata
        std::vector<ShaderProp> properties;

        // Multi-compile keyword sets (#pragma multi_compile / #pragma multi_compile_local)
        std::vector<std::vector<std::string>> multiCompileKeywords;

        // --- New extended data ---
        // Capability requirements from #pragma require
        std::vector<ShaderCapabilityRequirement> requirements;
        // Fallback shader path from #pragma fallback
        std::string fallbackShader;
        // SPIR-V specialization constants from #pragma specialize
        std::vector<ShaderSpecializationConstant> specializationConstants;
        // Centralized resource layout from ResourceLayout { } block
        std::vector<ResourceBinding> resourceLayout;
        // Named passes (Pass "name" { ... } blocks) -- stages reference passName
        std::vector<std::string> passNames;
        // SubShader descriptors -- one entry per SubShader { } block
        std::vector<SubShaderDesc> subShaders;
        // SubShader index each stage belongs to (-1 = top-level / legacy)
        std::vector<int> stageSubShaderIndex;
    };

    struct CompiledShader
    {
        std::string name;
        ShaderStage stage;
        ShaderLanguage language;
        std::vector<uint32_t> spirv;
        std::string entryPoint;
        glm::uvec3 workgroupSize = {1, 1, 1};
        bool hasWorkgroupSize = false;
    };

    // ShaderParser
    class ShaderParser
    {
    public:
        ShaderParser();
        ~ShaderParser();

        std::optional<ParsedShader> parse(const std::string &filepath);
        std::optional<ParsedShader> parseSource(const std::string &source, const std::string &name = "");
        std::optional<CompiledShader> compile(const ParsedShader &shader, ShaderStage stage,
                                              const std::vector<SF::Engine::Shader::Define> &defines = {});

        const std::string &getLastError() const { return lastError_; }

    private:
        void setError(const std::string &error);

        struct ParseContext
        {
            std::string source;
            size_t pos = 0;
            int line = 1;
            ParsedShader *shader = nullptr;
            // Tracks which SubShader block we're currently inside (-1 = none)
            int currentSubShader = -1;
        };

        // Top-level parse methods
        bool parseDeclaration(ParseContext &ctx);
        bool parseShaderBody(ParseContext &ctx);
        bool parseStageBlock(ParseContext &ctx, const std::string &passName,
                             const PassRenderState &inheritedState);
        bool parsePassBlock(ParseContext &ctx);
        bool parseSubShaderBlock(ParseContext &ctx);
        bool parsePropertiesBlock(ParseContext &ctx);
        bool parseResourceLayoutBlock(ParseContext &ctx);
        bool parsePragma(ParseContext &ctx);
        bool parseTagsBlock(ParseContext &ctx, std::map<std::string, std::string> &out);

        bool isShaderStageKeyword(const std::string &token);
        std::vector<ShaderStage> parseStageFilterList(ParseContext &ctx);

        // Property annotation helpers
        bool parseAnnotations(ParseContext &ctx, ShaderProp &prop);

        // Static extractors
        static bool extractWorkgroupSize(const std::string &source, glm::uvec3 &outSize);
        static std::optional<std::string> extractEntryPoint(const std::string &source);

        // Tokeniser helpers
        void skipWhitespace(ParseContext &ctx);
        std::string readToken(ParseContext &ctx);
        std::string peekToken(ParseContext &ctx);
        std::string readQuotedString(ParseContext &ctx);
        std::string readUntil(ParseContext &ctx, char delim);
        std::string stripComments(const std::string &source);

        // Compilation helpers
        std::string preprocessStage(const ParsedShader &shader,
                                    const ParsedShaderStage &stage,
                                    const std::vector<SF::Engine::Shader::Define> &defines);
        std::vector<uint32_t> compileGLSL(const std::string &source, ShaderStage stage,
                                          const std::string &entryPoint);
        std::vector<uint32_t> compileHLSL(const std::string &source, ShaderStage stage,
                                          const std::string &entryPoint);

        static std::string stageToDefine(ShaderStage stage);

        std::string lastError_;
        static bool glslangReady_;
    };

    // Helpers
    inline const char *stageToString(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return "vertex";
        case ShaderStage::Fragment:
            return "fragment";
        case ShaderStage::Compute:
            return "compute";
        case ShaderStage::Geometry:
            return "geometry";
        case ShaderStage::TessellationControl:
            return "tess_control";
        case ShaderStage::TessellationEvaluation:
            return "tess_eval";
        default:
            return "unknown";
        }
    }

    inline const char *propTypeToString(PropType t)
    {
        switch (t)
        {
        case PropType::Int:
            return "Int";
        case PropType::Float:
            return "Float";
        case PropType::Bool:
            return "Bool";
        case PropType::String:
            return "String";
        case PropType::Color:
            return "Color";
        case PropType::Vector:
            return "Vector";
        case PropType::Texture2D:
            return "Texture2D";
        case PropType::Texture3D:
            return "Texture3D";
        case PropType::TextureCube:
            return "TextureCube";
        case PropType::Range:
            return "Range";
        case PropType::Enum:
            return "Enum";
        case PropType::Gradient:
            return "Gradient";
        default:
            return "Unknown";
        }
    }

    inline std::optional<ShaderStage> stringToStage(const std::string &str)
    {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "vertexshader" || lower == "vertex")
            return ShaderStage::Vertex;
        if (lower == "fragmentshader" || lower == "pixelshader" ||
            lower == "fragment" || lower == "pixel")
            return ShaderStage::Fragment;
        if (lower == "computeshader" || lower == "compute")
            return ShaderStage::Compute;
        if (lower == "geometryshader" || lower == "geometry")
            return ShaderStage::Geometry;
        if (lower == "tessellationcontrol" || lower == "tesellationcontrol" ||
            lower == "tesscontrol" || lower == "hull")
            return ShaderStage::TessellationControl;
        if (lower == "tessellationeval" || lower == "tesellationeval" ||
            lower == "tesseval" || lower == "domain")
            return ShaderStage::TessellationEvaluation;
        return std::nullopt;
    }

} // namespace SF::Engine::Shaders
