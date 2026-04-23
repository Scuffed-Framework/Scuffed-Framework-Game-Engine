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
    // -------------------------------------------------------------------------
    // Public enums
    // -------------------------------------------------------------------------
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

    // -------------------------------------------------------------------------
    // Public structs - everything callers might need
    // -------------------------------------------------------------------------
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
        std::map<std::string, std::string> stringProps;
        std::map<std::string, int> intProps;
        std::map<std::string, float> floatProps;
        std::map<std::string, bool> boolProps;
    };

    struct CompiledShader
    {
        std::string name;
        ShaderStage stage;
        ShaderLanguage language;
        std::vector<uint32_t> spirv;
        std::string entryPoint;
        glm::uvec3 workgroupSize = {};
        bool hasWorkgroupSize = false;
    };

    // -------------------------------------------------------------------------
    // ShaderParser - no PIMPL, just normal class
    // -------------------------------------------------------------------------
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

        // Parsing state
        struct ParseContext
        {
            std::string source;
            size_t pos = 0;
            int line = 1;
            ParsedShader *shader = nullptr;
        };

        // Methods
        bool parseDeclaration(ParseContext &ctx);
        bool parseStageBlock(ParseContext &ctx);
        bool isShaderStageKeyword(const std::string &token);
        std::vector<ShaderStage> parseStageFilterList(ParseContext &ctx);

        void skipWhitespace(ParseContext &ctx);
        std::string readToken(ParseContext &ctx);
        std::string peekToken(ParseContext &ctx);
        std::string readQuotedString(ParseContext &ctx);
        std::string readUntil(ParseContext &ctx, char delim);
        std::string stripComments(const std::string &source);

        std::string preprocessStage(const ParsedShader &shader, const ParsedShaderStage &stage,
                                    const std::vector<SF::Engine::Shader::Define> &defines);
        std::vector<uint32_t> compileGLSL(const std::string &source, ShaderStage stage, const std::string &entryPoint);
        std::vector<uint32_t> compileHLSL(const std::string &source, ShaderStage stage, const std::string &entryPoint);

        static std::string stageToDefine(ShaderStage stage);

        std::string lastError_;
        static bool glslangReady_;
    };

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
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

    inline std::optional<ShaderStage> stringToStage(const std::string &str)
    {
        std::string lower = str;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "vertexshader" || lower == "vertex")
            return ShaderStage::Vertex;
        if (lower == "fragmentshader" || lower == "pixelshader" || lower == "fragment" || lower == "pixel")
            return ShaderStage::Fragment;
        if (lower == "computeshader" || lower == "compute")
            return ShaderStage::Compute;
        if (lower == "geometryshader" || lower == "geometry")
            return ShaderStage::Geometry;
        if (lower == "tessellationcontrol" || lower == "tesellationcontrol" || lower == "tesscontrol" || lower == "hull")
            return ShaderStage::TessellationControl;
        if (lower == "tessellationeval" || lower == "tesellationeval" || lower == "tesseval" || lower == "domain")
            return ShaderStage::TessellationEvaluation;
        return std::nullopt;
    }
}