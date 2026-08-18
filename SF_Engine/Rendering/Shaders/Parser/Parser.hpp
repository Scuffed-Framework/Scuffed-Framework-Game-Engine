#pragma once

#define VK_NO_PROTOTYPES

#include <Math/BasicMath.hpp>
#include <optional>
#include <string>
#include <vector>
#include "../Shader.hpp"

namespace SF::Engine::Shaders
{
    enum class ShaderLanguage
    {
        GLSL,  // GLSL-flavoured source, parsed by Slang's GLSL frontend
        HLSL,
        SLANG
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

    // One [shader("...")] entry point Slang found inside the file.
    // Populated by ShaderParser::discoverEntryPoints() -- never hand-authored.
    struct ShaderEntryPointInfo
    {
        std::string name;
        ShaderStage stage;
        glm::uvec3 workgroupSize = {1, 1, 1};
        bool hasWorkgroupSize = false;
    };

    struct ParsedShader
    {
        std::string name;     // derived from the filename -- no more `Shader "..."` header
        std::string filepath;
        ShaderLanguage language = ShaderLanguage::GLSL;
        std::string source;   // comment-stripped file text; #import/#include not yet expanded

        // Entry points Slang found via [shader("...")] attributes. Empty until
        // discoverEntryPoints() runs -- compile()/compileAll() do this lazily.
        std::vector<ShaderEntryPointInfo> entryPoints;

        // --- Metadata pulled from #pragma lines ---
        std::vector<ShaderCapabilityRequirement> requirements;
        std::string fallbackShader;
        std::vector<ShaderSpecializationConstant> specializationConstants;
        std::vector<std::vector<std::string>> multiCompileKeywords;

        // #pragma workgroup_size override -- takes precedence over whatever
        // the compute entry point's [numthreads]/layout(local_size_*) says.
        glm::uvec3 workgroupSizeOverride = {1, 1, 1};
        bool hasWorkgroupSizeOverride = false;
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

        // Compiles the file once (no extra defines) purely to ask Slang which
        // [shader("...")] entry points it contains. Populates shader.entryPoints.
        // compile()/compileAll() call this automatically if it hasn't been run.
        bool discoverEntryPoints(ParsedShader &shader);

        std::optional<CompiledShader> compile(const ParsedShader &shader, ShaderStage stage,
                                              const std::vector<SF::Engine::Shader::Define> &defines = {}, const std::string &entry = "");
        std::optional<std::vector<CompiledShader>> compileAll(const ParsedShader &shader,
                                              const std::vector<SF::Engine::Shader::Define> &defines = {});

        const std::string &getLastError() const { return lastError_; }

    private:
        void setError(const std::string &error);

        // Linear scan for #pragma metadata -- there's no block structure left to parse.
        void scanPragmas(ParsedShader &shader);
        // Injects defines/specialization-constant decls, resolves #import/#include,
        // returns source ready to hand to Slang.
        std::string preprocess(const ParsedShader &shader,
                               const std::vector<SF::Engine::Shader::Define> &defines);

        std::string lastError_;
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

} // namespace SF::Engine::Shaders