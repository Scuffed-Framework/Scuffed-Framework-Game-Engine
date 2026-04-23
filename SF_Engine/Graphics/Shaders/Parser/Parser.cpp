#include "Parser.hpp"
#include "ShaderIncludes.hpp"
#include "DirStackFileIncluder.hpp"

#include <glslang/Public/ShaderLang.h>
#include <glslang/Include/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <volk.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>

namespace SF::Engine::Shaders
{
    bool ShaderParser::glslangReady_ = true;

    // -------------------------------------------------------------------------
    // glslang resource table
    // -------------------------------------------------------------------------
    static const TBuiltInResource DefaultTBuiltInResource = {
        /* .MaxLights = */ 32,
        /* .MaxClipPlanes = */ 6,
        /* .MaxTextureUnits = */ 32,
        /* .MaxTextureCoords = */ 32,
        /* .MaxVertexAttribs = */ 64,
        /* .MaxVertexUniformComponents = */ 4096,
        /* .MaxVaryingFloats = */ 64,
        /* .MaxVertexTextureImageUnits = */ 32,
        /* .MaxCombinedTextureImageUnits = */ 80,
        /* .MaxTextureImageUnits = */ 32,
        /* .MaxFragmentUniformComponents = */ 4096,
        /* .MaxDrawBuffers = */ 32,
        /* .MaxVertexUniformVectors = */ 128,
        /* .MaxVaryingVectors = */ 8,
        /* .MaxFragmentUniformVectors = */ 16,
        /* .MaxVertexOutputVectors = */ 16,
        /* .MaxFragmentInputVectors = */ 15,
        /* .MinProgramTexelOffset = */ -8,
        /* .MaxProgramTexelOffset = */ 7,
        /* .MaxClipDistances = */ 8,
        /* .MaxComputeWorkGroupCountX = */ 65535,
        /* .MaxComputeWorkGroupCountY = */ 65535,
        /* .MaxComputeWorkGroupCountZ = */ 65535,
        /* .MaxComputeWorkGroupSizeX = */ 1024,
        /* .MaxComputeWorkGroupSizeY = */ 1024,
        /* .MaxComputeWorkGroupSizeZ = */ 64,
        /* .MaxComputeUniformComponents = */ 1024,
        /* .MaxComputeTextureImageUnits = */ 16,
        /* .MaxComputeImageUniforms = */ 8,
        /* .MaxComputeAtomicCounters = */ 8,
        /* .MaxComputeAtomicCounterBuffers = */ 1,
        /* .MaxVaryingComponents = */ 60,
        /* .MaxVertexOutputComponents = */ 64,
        /* .MaxGeometryInputComponents = */ 64,
        /* .MaxGeometryOutputComponents = */ 128,
        /* .MaxFragmentInputComponents = */ 128,
        /* .MaxImageUnits = */ 8,
        /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
        /* .MaxCombinedShaderOutputResources = */ 8,
        /* .MaxImageSamples = */ 0,
        /* .MaxVertexImageUniforms = */ 0,
        /* .MaxTessControlImageUniforms = */ 0,
        /* .MaxTessEvaluationImageUniforms = */ 0,
        /* .MaxGeometryImageUniforms = */ 0,
        /* .MaxFragmentImageUniforms = */ 8,
        /* .MaxCombinedImageUniforms = */ 8,
        /* .MaxGeometryTextureImageUnits = */ 16,
        /* .MaxGeometryOutputVertices = */ 256,
        /* .MaxGeometryTotalOutputComponents = */ 1024,
        /* .MaxGeometryUniformComponents = */ 1024,
        /* .MaxGeometryVaryingComponents = */ 64,
        /* .MaxTessControlInputComponents = */ 128,
        /* .MaxTessControlOutputComponents = */ 128,
        /* .MaxTessControlTextureImageUnits = */ 16,
        /* .MaxTessControlUniformComponents = */ 1024,
        /* .MaxTessControlTotalOutputComponents = */ 4096,
        /* .MaxTessEvaluationInputComponents = */ 128,
        /* .MaxTessEvaluationOutputComponents = */ 128,
        /* .MaxTessEvaluationTextureImageUnits = */ 16,
        /* .MaxTessEvaluationUniformComponents = */ 1024,
        /* .MaxTessPatchComponents = */ 120,
        /* .MaxPatchVertices = */ 32,
        /* .MaxTessGenLevel = */ 64,
        /* .MaxViewports = */ 16,
        /* .MaxVertexAtomicCounters = */ 0,
        /* .MaxTessControlAtomicCounters = */ 0,
        /* .MaxTessEvaluationAtomicCounters = */ 0,
        /* .MaxGeometryAtomicCounters = */ 0,
        /* .MaxFragmentAtomicCounters = */ 8,
        /* .MaxCombinedAtomicCounters = */ 8,
        /* .MaxAtomicCounterBindings = */ 1,
        /* .MaxVertexAtomicCounterBuffers = */ 0,
        /* .MaxTessControlAtomicCounterBuffers = */ 0,
        /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
        /* .MaxGeometryAtomicCounterBuffers = */ 0,
        /* .MaxFragmentAtomicCounterBuffers = */ 1,
        /* .MaxCombinedAtomicCounterBuffers = */ 1,
        /* .MaxAtomicCounterBufferSize = */ 16384,
        /* .MaxTransformFeedbackBuffers = */ 4,
        /* .MaxTransformFeedbackInterleavedComponents = */ 64,
        /* .MaxCullDistances = */ 8,
        /* .MaxCombinedClipAndCullDistances = */ 8,
        /* .MaxSamples = */ 4,
        /* .maxMeshOutputVerticesNV = */ 256,
        /* .maxMeshOutputPrimitivesNV = */ 512,
        /* .maxMeshWorkGroupSizeX_NV = */ 32,
        /* .maxMeshWorkGroupSizeY_NV = */ 1,
        /* .maxMeshWorkGroupSizeZ_NV = */ 1,
        /* .maxTaskWorkGroupSizeX_NV = */ 32,
        /* .maxTaskWorkGroupSizeY_NV = */ 1,
        /* .maxTaskWorkGroupSizeZ_NV = */ 1,
        /* .maxMeshViewCountNV = */ 4,
        /* .maxMeshOutputVerticesEXT = */ 256,
        /* .maxMeshOutputPrimitivesEXT = */ 256,
        /* .maxMeshWorkGroupSizeX_EXT = */ 128,
        /* .maxMeshWorkGroupSizeY_EXT = */ 128,
        /* .maxMeshWorkGroupSizeZ_EXT = */ 128,
        /* .maxTaskWorkGroupSizeX_EXT = */ 128,
        /* .maxTaskWorkGroupSizeY_EXT = */ 128,
        /* .maxTaskWorkGroupSizeZ_EXT = */ 128,
        /* .maxMeshViewCountEXT = */ 4,
        /* .maxDualSourceDrawBuffersEXT = */ 1,

        /* .limits = */ {
            /* .nonInductiveForLoops = */ 1,
            /* .whileLoops = */ 1,
            /* .doWhileLoops = */ 1,
            /* .generalUniformIndexing = */ 1,
            /* .generalAttributeMatrixVectorIndexing = */ 1,
            /* .generalVaryingIndexing = */ 1,
            /* .generalSamplerIndexing = */ 1,
            /* .generalVariableIndexing = */ 1,
            /* .generalConstantMatrixVectorIndexing = */ 1,
        }};

    static const TBuiltInResource *GetDefaultResources() { return &DefaultTBuiltInResource; }

    static EShLanguage ToGlslangStage(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return EShLangVertex;
        case ShaderStage::Fragment:
            return EShLangFragment;
        case ShaderStage::Compute:
            return EShLangCompute;
        case ShaderStage::Geometry:
            return EShLangGeometry;
        case ShaderStage::TessellationControl:
            return EShLangTessControl;
        case ShaderStage::TessellationEvaluation:
            return EShLangTessEvaluation;
        default:
            return EShLangVertex;
        }
    }

    // =========================================================================
    // ShaderParser
    // =========================================================================
    ShaderParser::ShaderParser() = default;
    ShaderParser::~ShaderParser() = default;

    void ShaderParser::setError(const std::string &error)
    {
        lastError_ = error;
        std::cerr << "ShaderParser Error: " << error << std::endl;
    }

    std::optional<ParsedShader> ShaderParser::parse(const std::string &filepath)
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            setError("Failed to open file: " + filepath);
            return std::nullopt;
        }
        std::stringstream buf;
        buf << file.rdbuf();
        auto result = parseSource(buf.str(), filepath);
        if (result)
            result->filepath = filepath;
        return result;
    }

    std::optional<ParsedShader> ShaderParser::parseSource(const std::string &source,
                                                          const std::string &name)
    {
        ParsedShader shader;
        shader.name = name.empty() ? "unnamed" : name;
        shader.language = ShaderLanguage::GLSL;

        ParseContext ctx;
        ctx.source = stripComments(source);
        ctx.shader = &shader;
        skipWhitespace(ctx);

        if (!parseDeclaration(ctx))
            return std::nullopt;

        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
        {
            setError("Expected '{' after shader declaration");
            return std::nullopt;
        }
        ctx.pos++;

        while (ctx.pos < ctx.source.length())
        {
            skipWhitespace(ctx);
            if (ctx.pos >= ctx.source.length())
                break;
            if (ctx.source[ctx.pos] == '}')
            {
                ctx.pos++;
                break;
            }

            std::string token = peekToken(ctx);

            if (token == "#import" || token == "#include")
            {
                bool isImport = (token == "#import");
                readToken(ctx);
                std::string inc = readQuotedString(ctx);
                if (inc.empty())
                    inc = readToken(ctx);

                ShaderIncludeEntry entry;
                entry.path = inc;
                entry.isImport = isImport;
                entry.stages = parseStageFilterList(ctx);
                shader.includes.push_back(entry);
            }
            else if (token == "#pragma")
            {
                readToken(ctx);
                std::string pragma = readUntil(ctx, '\n');
                if (pragma.find("hlsl") != std::string::npos)
                    shader.language = ShaderLanguage::HLSL;
            }
            else if (isShaderStageKeyword(token))
            {
                if (!parseStageBlock(ctx))
                    return std::nullopt;
            }
            else if (token == "inout" || token == "uniform" ||
                     token == "in" || token == "out")
            {
                readUntil(ctx, ';');
            }
            else
            {
                ctx.pos++;
            }
        }
        return shader;
    }

    std::optional<CompiledShader> ShaderParser::compile(
        const ParsedShader &shader, ShaderStage stage,
        const std::vector<SF::Engine::Shader::Define> &defines)
    {
        const ParsedShaderStage *stagePtr = nullptr;
        for (const auto &s : shader.stages)
            if (s.stage == stage)
            {
                stagePtr = &s;
                break;
            }

        if (!stagePtr)
        {
            setError("Stage not found in shader: " + std::string(stageToString(stage)));
            return std::nullopt;
        }

        std::string processed = preprocessStage(shader, *stagePtr, defines);

        std::vector<uint32_t> spirv =
            (shader.language == ShaderLanguage::GLSL)
                ? compileGLSL(processed, stage, stagePtr->entryPoint)
                : compileHLSL(processed, stage, stagePtr->entryPoint);

        if (spirv.empty())
        {
            std::cerr << "ShaderParser: empty SPIRV for stage " << stageToString(stage)
                      << " in '" << shader.name << "'. Error: " << lastError_ << std::endl;
            return std::nullopt;
        }

        CompiledShader compiled;
        compiled.name = shader.name;
        compiled.stage = stage;
        compiled.language = shader.language;
        compiled.spirv = std::move(spirv);
        compiled.entryPoint = stagePtr->entryPoint;
        return compiled;
    }

    // =========================================================================
    // Parsing
    // =========================================================================

    bool ShaderParser::parseDeclaration(ParseContext &ctx)
    {
        while (true)
        {
            skipWhitespace(ctx);
            std::string peeked = peekToken(ctx);
            if (peeked != "#import" && peeked != "#include")
                break;

            bool isImport = (peeked == "#import");
            readToken(ctx);
            std::string inc = readQuotedString(ctx);
            if (inc.empty())
                inc = readToken(ctx);

            ShaderIncludeEntry entry;
            entry.path = inc;
            entry.isImport = isImport;
            entry.stages = parseStageFilterList(ctx);
            ctx.shader->includes.push_back(entry);
        }

        skipWhitespace(ctx);
        std::string token = readToken(ctx);
        if (token != "Shader" && token != "shader")
        {
            setError("Expected 'Shader' declaration, got: " + token);
            return false;
        }
        skipWhitespace(ctx);
        ctx.shader->name = readQuotedString(ctx);
        if (ctx.shader->name.empty())
        {
            setError("Shader name cannot be empty");
            return false;
        }
        return true;
    }

    bool ShaderParser::isShaderStageKeyword(const std::string &token)
    {
        return token == "VertexShader" || token == "FragmentShader" ||
               token == "ComputeShader" || token == "GeometryShader" ||
               token == "TessellationControl" || token == "TessellationEval" ||
               token == "TesellationControl" || token == "TesellationEval";
    }

    bool ShaderParser::parseStageBlock(ParseContext &ctx)
    {
        skipWhitespace(ctx);
        std::string stageStr = readToken(ctx);
        auto stageOpt = stringToStage(stageStr);
        if (!stageOpt)
        {
            setError("Unknown shader stage: " + stageStr);
            return false;
        }

        ParsedShaderStage stage;
        stage.stage = *stageOpt;
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
        {
            setError("Expected '{' after stage declaration");
            return false;
        }
        ctx.pos++;

        int braceDepth = 1;
        size_t start = ctx.pos;
        while (ctx.pos < ctx.source.length() && braceDepth > 0)
        {
            if (ctx.source[ctx.pos] == '{')
                braceDepth++;
            else if (ctx.source[ctx.pos] == '}')
                braceDepth--;
            if (braceDepth > 0)
                ctx.pos++;
        }
        stage.source = ctx.source.substr(start, ctx.pos - start);
        ctx.pos++;
        ctx.shader->stages.push_back(stage);
        return true;
    }

    std::vector<ShaderStage> ShaderParser::parseStageFilterList(ParseContext &ctx)
    {
        size_t saved = ctx.pos;
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '[')
        {
            ctx.pos = saved;
            return {};
        }
        ctx.pos++;

        std::vector<ShaderStage> result;
        while (ctx.pos < ctx.source.length())
        {
            skipWhitespace(ctx);
            if (ctx.pos >= ctx.source.length())
                break;
            if (ctx.source[ctx.pos] == ']')
            {
                ctx.pos++;
                break;
            }
            if (ctx.source[ctx.pos] == ',')
            {
                ctx.pos++;
                continue;
            }

            std::string tok = readToken(ctx);
            if (tok.empty())
            {
                ctx.pos++;
                continue;
            }
            auto s = stringToStage(tok);
            if (s)
                result.push_back(*s);
        }
        return result;
    }

    // =========================================================================
    // Token helpers
    // =========================================================================

    void ShaderParser::skipWhitespace(ParseContext &ctx)
    {
        while (ctx.pos < ctx.source.length() && std::isspace(ctx.source[ctx.pos]))
        {
            if (ctx.source[ctx.pos] == '\n')
                ctx.line++;
            ctx.pos++;
        }
    }

    std::string ShaderParser::peekToken(ParseContext &ctx)
    {
        size_t saved = ctx.pos;
        std::string t = readToken(ctx);
        ctx.pos = saved;
        return t;
    }

    std::string ShaderParser::readToken(ParseContext &ctx)
    {
        skipWhitespace(ctx);
        std::string token;
        while (ctx.pos < ctx.source.length())
        {
            char c = ctx.source[ctx.pos];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '#')
            {
                token += c;
                ctx.pos++;
            }
            else
                break;
        }
        return token;
    }

    std::string ShaderParser::readQuotedString(ParseContext &ctx)
    {
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '"')
            return "";
        ctx.pos++;
        std::string str;
        while (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] != '"')
            str += ctx.source[ctx.pos++];
        if (ctx.pos < ctx.source.length())
            ctx.pos++;
        return str;
    }

    std::string ShaderParser::readUntil(ParseContext &ctx, char delim)
    {
        std::string result;
        while (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] != delim)
            result += ctx.source[ctx.pos++];
        if (ctx.pos < ctx.source.length())
            ctx.pos++;
        return result;
    }

    std::string ShaderParser::stripComments(const std::string &source)
    {
        return ShaderIncludeUtils::stripComments(source);
    }

    // =========================================================================
    // Preprocessing
    // =========================================================================

    std::string ShaderParser::preprocessStage(
        const ParsedShader &shader,
        const ParsedShaderStage &stage,
        const std::vector<SF::Engine::Shader::Define> &defines)
    {
        std::string result;
        if (shader.language == ShaderLanguage::GLSL)
            result += "#version 450 core\n";

        for (const auto &[name, value] : defines)
        {
            result += "#define " + name;
            if (!value.empty())
                result += " " + value;
            result += "\n";
        }

        result += stageToDefine(stage.stage) + "\n\n";

        for (const auto &entry : shader.includes)
        {
            bool applies = entry.stages.empty();
            if (!applies)
                for (auto s : entry.stages)
                    if (s == stage.stage)
                    {
                        applies = true;
                        break;
                    }

            if (applies)
                result += "#include \"" + entry.path + "\"\n";
        }

        std::string stageBody = result + stage.source;

        {
            size_t vpos = stageBody.find("#version");
            while (vpos != std::string::npos)
            {
                size_t end = stageBody.find('\n', vpos);
                stageBody.erase(vpos, end == std::string::npos ? std::string::npos : end - vpos + 1);
                vpos = stageBody.find("#version");
            }
            stageBody = "#version 450 core\n" + stageBody;
        }

        ShaderIncludeResolver resolver;
        if (!shader.filepath.empty())
            resolver.addIncludeDirectory(ShaderIncludeUtils::getDirectory(shader.filepath));

        resolver.addIncludeDirectory(".");
        resolver.addIncludeDirectory("./Shaders");
        // resolver.addIncludeDirectory("./Shaders/Include");
        resolver.addIncludeDirectory("./Shaders/Clouds");
        resolver.addIncludeDirectory("./Shaders/Common");
        resolver.addIncludeDirectory("./Shaders/Atmosphere");
        resolver.addIncludeDirectory("./Shaders/Lighting");

        std::string basePath = shader.filepath.empty()
                                   ? "./Shaders"
                                   : ShaderIncludeUtils::getDirectory(shader.filepath);

        auto resolved = resolver.resolveIncludes(stageBody, basePath, true);

        if (!resolved)
        {
            std::cerr << "Warning: Failed to resolve includes: "
                      << resolver.getLastError() << std::endl;
            std::string safe;
            std::istringstream ss(stageBody);
            std::string ln;
            while (std::getline(ss, ln))
            {
                size_t f = ln.find_first_not_of(" \t");
                if (f == std::string::npos || ln.substr(f, 7) != "#import")
                    safe += ln + "\n";
            }
            return safe;
        }

        return *resolved;
    }

    // =========================================================================
    // Compilation
    // =========================================================================

    std::vector<uint32_t> ShaderParser::compileGLSL(const std::string &source,
                                                    ShaderStage stage,
                                                    const std::string &entryPoint)
    {
        EShLanguage glslStage = ToGlslangStage(stage);
        glslang::TShader glShader(glslStage);
        const char *src = source.c_str();
        glShader.setStrings(&src, 1);
        glShader.setEnvInput(glslang::EShSourceGlsl, glslStage, glslang::EShClientVulkan, 100);
        glShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        glShader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        const TBuiltInResource *res = GetDefaultResources();
        EShMessages msg = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
        DirStackFileIncluder includer;
        std::string preprocessed;

        if (!glShader.preprocess(res, 100, ENoProfile, false, false, msg, &preprocessed, includer))
        {
            setError("GLSL preprocess failed:\n" + std::string(glShader.getInfoLog()));
            return {};
        }
        const char *pp = preprocessed.c_str();
        glShader.setStrings(&pp, 1);
        if (!glShader.parse(res, 100, false, msg))
        {
            setError("GLSL parse failed:\n" + std::string(glShader.getInfoLog()));
            return {};
        }
        glslang::TProgram program;
        program.addShader(&glShader);
        if (!program.link(msg))
        {
            setError("GLSL link failed:\n" + std::string(program.getInfoLog()));
            return {};
        }
        std::vector<uint32_t> spirv;
        spv::SpvBuildLogger logger;
        glslang::SpvOptions opts;
        glslang::GlslangToSpv(*program.getIntermediate(glslStage), spirv, &logger, &opts);
        return spirv;
    }

    std::vector<uint32_t> ShaderParser::compileHLSL(const std::string &source,
                                                    ShaderStage stage,
                                                    const std::string &entryPoint)
    {
        EShLanguage glslStage = ToGlslangStage(stage);
        glslang::TShader glShader(glslStage);
        const char *src = source.c_str();
        glShader.setStrings(&src, 1);
        glShader.setEntryPoint(entryPoint.empty() ? "main" : entryPoint.c_str());
        glShader.setEnvInput(glslang::EShSourceHlsl, glslStage, glslang::EShClientVulkan, 100);
        glShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        glShader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        const TBuiltInResource *res = GetDefaultResources();
        EShMessages msg = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);
        DirStackFileIncluder includer;
        std::string preprocessed;

        if (!glShader.preprocess(res, 100, ENoProfile, false, false, msg, &preprocessed, includer))
        {
            setError("HLSL preprocess failed:\n" + std::string(glShader.getInfoLog()));
            return {};
        }
        const char *pp = preprocessed.c_str();
        glShader.setStrings(&pp, 1);
        if (!glShader.parse(res, 100, false, msg))
        {
            setError("HLSL parse failed:\n" + std::string(glShader.getInfoLog()));
            return {};
        }
        glslang::TProgram program;
        program.addShader(&glShader);
        if (!program.link(msg))
        {
            setError("HLSL link failed:\n" + std::string(program.getInfoLog()));
            return {};
        }
        std::vector<uint32_t> spirv;
        spv::SpvBuildLogger logger;
        glslang::SpvOptions opts;
        glslang::GlslangToSpv(*program.getIntermediate(glslStage), spirv, &logger, &opts);
        return spirv;
    }

    std::string ShaderParser::stageToDefine(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return "#define VERTEX_SHADER";
        case ShaderStage::Fragment:
            return "#define FRAGMENT_SHADER";
        case ShaderStage::Compute:
            return "#define COMPUTE_SHADER";
        case ShaderStage::Geometry:
            return "#define GEOMETRY_SHADER";
        case ShaderStage::TessellationControl:
            return "#define TESS_CONTROL_SHADER";
        case ShaderStage::TessellationEvaluation:
            return "#define TESS_EVAL_SHADER";
        default:
            return "";
        }
    }

} // namespace SF::Engine::Shaders