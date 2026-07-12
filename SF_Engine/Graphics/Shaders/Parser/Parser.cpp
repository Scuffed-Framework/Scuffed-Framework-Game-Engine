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

    // ShaderParser

    ShaderParser::ShaderParser() = default;
    ShaderParser::~ShaderParser() = default;

    void ShaderParser::setError(const std::string &error)
    {
        lastError_ = error;
        std::cerr << "ShaderParser Error: " << error << std::endl;
    }

    // Public entry points

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
        ctx.currentSubShader = -1;
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

        if (!parseShaderBody(ctx))
            return std::nullopt;

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
        compiled.workgroupSize = stagePtr->workgroupSize;
        compiled.hasWorkgroupSize = stagePtr->hasWorkgroupSize;
        return compiled;
    }

    // Top-level body parser
    // Handles: Properties, #pragma, #import/#include, SubShader, Pass,
    //          stage blocks, ResourceLayout, #Section.
    // All constructs are valid at both the top level and nested inside
    // SubShader blocks, so SubShader delegates back here after noting the LOD.

    bool ShaderParser::parseShaderBody(ParseContext &ctx)
    {
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
                ctx.shader->includes.push_back(entry);
            }
            else if (token == "#pragma")
            {
                if (!parsePragma(ctx))
                    return false;
            }
            else if (token == "Properties")
            {
                readToken(ctx);
                if (!parsePropertiesBlock(ctx))
                    return false;
            }
            else if (token == "ResourceLayout")
            {
                readToken(ctx);
                if (!parseResourceLayoutBlock(ctx))
                    return false;
            }
            else if (token == "SubShader")
            {
                if (!parseSubShaderBlock(ctx))
                    return false;
            }
            else if (token == "Pass")
            {
                if (!parsePassBlock(ctx))
                    return false;
            }
            else if (isShaderStageKeyword(token))
            {
                // Legacy / bare stage block (no enclosing Pass)
                if (!parseStageBlock(ctx, /*passName=*/"", /*inheritedState=*/{}))
                    return false;
            }
            else if (token == "inout" || token == "uniform" ||
                     token == "in" || token == "out")
            {
                readUntil(ctx, ';');
            }
            else if (token == "#Section")
            {
                readToken(ctx);
                readUntil(ctx, '\n');
            }
            else
            {
                ctx.pos++;
            }
        }
        return true;
    }

    // SubShader { LOD N  Tags { ... }  <body> }

    bool ShaderParser::parseSubShaderBlock(ParseContext &ctx)
    {
        readToken(ctx); // consume "SubShader"
        skipWhitespace(ctx);

        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
        {
            setError("Expected '{' after SubShader");
            return false;
        }
        ctx.pos++;

        SubShaderDesc desc;
        int savedSubShader = ctx.currentSubShader;
        ctx.currentSubShader = static_cast<int>(ctx.shader->subShaders.size());
        ctx.shader->subShaders.push_back(desc); // placeholder; updated in place below

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

            std::string tok = peekToken(ctx);

            if (tok == "LOD")
            {
                readToken(ctx); // consume "LOD"
                skipWhitespace(ctx);
                std::string val = readToken(ctx);
                try
                {
                    ctx.shader->subShaders[ctx.currentSubShader].lod = std::stoi(val);
                }
                catch (...)
                {
                }
            }
            else if (tok == "Tags")
            {
                readToken(ctx);
                parseTagsBlock(ctx, ctx.shader->subShaders[ctx.currentSubShader].tags);
            }
            else if (tok == "Pass")
            {
                if (!parsePassBlock(ctx))
                    return false;
            }
            else if (isShaderStageKeyword(tok))
            {
                if (!parseStageBlock(ctx, "", {}))
                    return false;
            }
            else if (tok == "#pragma")
            {
                if (!parsePragma(ctx))
                    return false;
            }
            else if (tok == "#import" || tok == "#include")
            {
                bool isImport = (tok == "#import");
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
            else
            {
                ctx.pos++;
            }
        }

        ctx.currentSubShader = savedSubShader;
        return true;
    }

    // Pass "name" { Tags { } Cull/ZWrite/Blend  <stage blocks> }

    bool ShaderParser::parsePassBlock(ParseContext &ctx)
    {
        readToken(ctx); // consume "Pass"
        skipWhitespace(ctx);

        // Optional pass name (quoted string)
        std::string passName;
        if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '"')
            passName = readQuotedString(ctx);

        if (!passName.empty())
        {
            // Register the name if not already known
            if (std::find(ctx.shader->passNames.begin(), ctx.shader->passNames.end(), passName) == ctx.shader->passNames.end())
                ctx.shader->passNames.push_back(passName);
        }

        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
        {
            setError("Expected '{' after Pass declaration");
            return false;
        }
        ctx.pos++;

        PassRenderState state;

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

            std::string tok = peekToken(ctx);

            if (tok == "Tags")
            {
                readToken(ctx);
                parseTagsBlock(ctx, state.tags);
            }
            else if (tok == "Cull")
            {
                readToken(ctx);
                skipWhitespace(ctx);
                state.cullMode = readToken(ctx);
            }
            else if (tok == "ZWrite")
            {
                readToken(ctx);
                skipWhitespace(ctx);
                state.zWrite = readToken(ctx);
            }
            else if (tok == "Blend")
            {
                readToken(ctx);
                skipWhitespace(ctx);
                state.blendSrc = readToken(ctx);
                skipWhitespace(ctx);
                state.blendDst = readToken(ctx);
            }
            else if (isShaderStageKeyword(tok))
            {
                if (!parseStageBlock(ctx, passName, state))
                    return false;
            }
            else if (tok == "#pragma")
            {
                if (!parsePragma(ctx))
                    return false;
            }
            else
            {
                ctx.pos++;
            }
        }
        return true;
    }

    // Tags { "Key" = "Value" ... }

    bool ShaderParser::parseTagsBlock(ParseContext &ctx, std::map<std::string, std::string> &out)
    {
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
            return true; // Tags block is always optional

        ctx.pos++; // consume '{'

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

            std::string key;
            if (ctx.source[ctx.pos] == '"')
                key = readQuotedString(ctx);
            else
                key = readToken(ctx);

            if (key.empty())
            {
                ctx.pos++;
                continue;
            }

            skipWhitespace(ctx);
            if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '=')
                ctx.pos++;

            skipWhitespace(ctx);
            std::string val;
            if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '"')
                val = readQuotedString(ctx);
            else
                val = readToken(ctx);

            out[key] = val;
        }
        return true;
    }

    // ResourceLayout { [set, binding] type Name [: qualifiers] ... }

    bool ShaderParser::parseResourceLayoutBlock(ParseContext &ctx)
    {
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
        {
            setError("Expected '{' after ResourceLayout");
            return false;
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

            // Expect [set, binding]
            if (ctx.source[ctx.pos] != '[')
            {
                readUntil(ctx, '\n');
                continue;
            }
            ctx.pos++; // consume '['

            std::string setStr = readToken(ctx);
            skipWhitespace(ctx);
            if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == ',')
                ctx.pos++;
            std::string bindingStr = readToken(ctx);
            skipWhitespace(ctx);
            if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == ']')
                ctx.pos++;

            skipWhitespace(ctx);
            std::string resType = readToken(ctx);
            skipWhitespace(ctx);
            std::string resName = readToken(ctx);
            skipWhitespace(ctx);

            std::string qualifiers;
            if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == ':')
            {
                ctx.pos++;
                qualifiers = readUntil(ctx, '\n');
                // Trim leading/trailing whitespace from qualifiers
                size_t qs = qualifiers.find_first_not_of(" \t");
                size_t qe = qualifiers.find_last_not_of(" \t\r");
                if (qs != std::string::npos)
                    qualifiers = qualifiers.substr(qs, qe - qs + 1);
                else
                    qualifiers.clear();
            }
            else if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] != '}')
            {
                readUntil(ctx, '\n');
            }

            if (resType.empty() || resName.empty())
                continue;

            ResourceBinding rb;
            try
            {
                rb.set = static_cast<uint32_t>(std::stoul(setStr));
            }
            catch (...)
            {
            }
            try
            {
                rb.binding = static_cast<uint32_t>(std::stoul(bindingStr));
            }
            catch (...)
            {
            }
            rb.resourceType = resType;
            rb.name = resName;
            rb.qualifiers = qualifiers;
            ctx.shader->resourceLayout.push_back(rb);
        }
        return true;
    }

    // Parsing helpers

    bool ShaderParser::parseDeclaration(ParseContext &ctx)
    {
        // Allow top-level #import/#include before the Shader declaration
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
               token == "TessCtrl" || token == "TessEval";
    }

    bool ShaderParser::parseStageBlock(ParseContext &ctx, const std::string &passName,
                                       const PassRenderState &inheritedState)
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
        stage.passName = passName;
        stage.renderState = inheritedState;

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
        stage.source = ShaderIncludeUtils::stripSectionDirectives(stage.source);
        ctx.pos++;

        // Extract entry point override from source: #pragma entry <name>
        auto ep = extractEntryPoint(stage.source);
        if (ep)
            stage.entryPoint = *ep;

        // Extract workgroup size for compute stages.
        // #pragma workgroup_size inside the block overrides layout(local_size_*).
        if (stage.stage == ShaderStage::Compute)
        {
            // First try layout(local_size_*) extraction
            stage.hasWorkgroupSize = extractWorkgroupSize(stage.source, stage.workgroupSize);

            // Then check for in-block #pragma workgroup_size which takes precedence
            std::istringstream ss(stage.source);
            std::string line;
            while (std::getline(ss, line))
            {
                size_t f = line.find_first_not_of(" \t");
                if (f == std::string::npos || line[f] != '#')
                    continue;
                size_t p = f + 1;
                while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
                    p++;
                if (line.substr(p, 6) != "pragma")
                    continue;
                p += 6;
                while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
                    p++;
                if (line.substr(p, 14) != "workgroup_size")
                    continue;
                p += 14;
                std::istringstream args(line.substr(p));
                uint32_t x = 1, y = 1, z = 1;
                args >> x;
                if (!(args >> y))
                    y = 1;
                if (!(args >> z))
                    z = 1;
                stage.workgroupSize = {x, y, z};
                stage.hasWorkgroupSize = true;
                break; // first occurrence wins
            }
        }

        // Track which SubShader this stage belongs to
        ctx.shader->stageSubShaderIndex.push_back(ctx.currentSubShader);
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

    // Token helpers

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

    // #pragma handler
    //
    // Supported pragmas (new additions marked **):
    //   hlsl                           -- switch parser to HLSL mode
    //   entry <name>                   -- set entry point
    //   workgroup_size <x> [y] [z]     -- explicit workgroup size override
    //   multi_compile  <KW...>         -- register multi-compile keyword set
    //   multi_compile_local <KW...>    -- same, local variant
    //   once                           -- silently ignored (include resolver handles it)
    //   ** require <feature> [tier]    -- add capability requirement
    //   ** fallback "path"             -- set fallback shader path
    //   ** specialize NAME = default   -- declare SPIR-V specialization constant
    //   anything else                  -- silently ignored for forward compat

    bool ShaderParser::parsePragma(ParseContext &ctx)
    {
        readToken(ctx); // consume "#pragma"
        std::string line = readUntil(ctx, '\n');

        size_t s = line.find_first_not_of(" \t");
        if (s == std::string::npos)
            return true;
        line = line.substr(s);

        auto split = [](const std::string &str) -> std::vector<std::string>
        {
            std::vector<std::string> tokens;
            std::istringstream ss(str);
            std::string t;
            while (ss >> t)
                tokens.push_back(t);
            return tokens;
        };

        auto tokens = split(line);
        if (tokens.empty())
            return true;

        const std::string &directive = tokens[0];

        if (directive == "hlsl")
        {
            ctx.shader->language = ShaderLanguage::HLSL;
        }
        else if (directive == "entry")
        {
            if (tokens.size() >= 2 && !ctx.shader->stages.empty())
                ctx.shader->stages.back().entryPoint = tokens[1];
        }
        else if (directive == "workgroup_size")
        {
            // Top-level override; in-block handling in parseStageBlock takes precedence.
            if (tokens.size() >= 2 && !ctx.shader->stages.empty())
            {
                auto &st = ctx.shader->stages.back();
                if (st.stage == ShaderStage::Compute)
                {
                    st.workgroupSize.x = static_cast<uint32_t>(std::stoul(tokens[1]));
                    st.workgroupSize.y = tokens.size() >= 3
                                             ? static_cast<uint32_t>(std::stoul(tokens[2]))
                                             : 1u;
                    st.workgroupSize.z = tokens.size() >= 4
                                             ? static_cast<uint32_t>(std::stoul(tokens[3]))
                                             : 1u;
                    st.hasWorkgroupSize = true;
                }
            }
        }
        else if (directive == "multi_compile" || directive == "multi_compile_local")
        {
            if (tokens.size() >= 2)
            {
                std::vector<std::string> kws(tokens.begin() + 1, tokens.end());
                ctx.shader->multiCompileKeywords.push_back(std::move(kws));
            }
        }
        // ---- New pragmas ----
        else if (directive == "require")
        {
            // #pragma require <feature> [tier]
            if (tokens.size() >= 2)
            {
                ShaderCapabilityRequirement req;
                req.feature = tokens[1];
                if (tokens.size() >= 3)
                    try
                    {
                        req.tier = std::stoi(tokens[2]);
                    }
                    catch (...)
                    {
                    }
                ctx.shader->requirements.push_back(req);
            }
        }
        else if (directive == "fallback")
        {
            // #pragma fallback "ShaderPath"
            // The path may be a quoted string or a bare token after the directive.
            // Re-scan from the raw line since readUntil has already consumed it.
            size_t q = line.find('"');
            if (q != std::string::npos)
            {
                size_t q2 = line.find('"', q + 1);
                if (q2 != std::string::npos)
                    ctx.shader->fallbackShader = line.substr(q + 1, q2 - q - 1);
            }
            else if (tokens.size() >= 2)
            {
                ctx.shader->fallbackShader = tokens[1];
            }
        }
        else if (directive == "specialize")
        {
            // #pragma specialize NAME = default_value
            // tokens: ["specialize", "NAME", "=", "value"]  or  ["specialize", "NAME=value"]
            if (tokens.size() >= 2)
            {
                ShaderSpecializationConstant sc;
                sc.constantId = static_cast<uint32_t>(ctx.shader->specializationConstants.size());

                // Handle both "NAME = val" and "NAME=val"
                std::string nameEqVal = tokens[1];
                size_t eq = nameEqVal.find('=');
                if (eq != std::string::npos)
                {
                    sc.name = nameEqVal.substr(0, eq);
                    sc.defaultValue = nameEqVal.substr(eq + 1);
                    // Trim
                    while (!sc.defaultValue.empty() && std::isspace(sc.defaultValue.front()))
                        sc.defaultValue.erase(sc.defaultValue.begin());
                }
                else
                {
                    sc.name = nameEqVal;
                    // Skip "=" token
                    size_t valIdx = 2;
                    if (tokens.size() > 2 && tokens[2] == "=")
                        valIdx = 3;
                    if (valIdx < tokens.size())
                        sc.defaultValue = tokens[valIdx];
                }

                if (!sc.name.empty())
                    ctx.shader->specializationConstants.push_back(sc);
            }
        }
        // else: unknown pragma -- silently ignored for forward compatibility

        return true;
    }

    // @annotation parser
    // Reads zero or more @annotation(...) or @annotation("...") lines that
    // appear before a property declaration and populates the prop struct.
    // Returns with ctx positioned at the first non-annotation token.

    bool ShaderParser::parseAnnotations(ParseContext &ctx, ShaderProp &prop)
    {
        while (true)
        {
            size_t saved = ctx.pos;
            skipWhitespace(ctx);
            if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '@')
            {
                ctx.pos = saved;
                break;
            }
            ctx.pos++; // consume '@'

            std::string annotName = readToken(ctx);
            skipWhitespace(ctx);

            std::string annotValue;
            if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '(')
            {
                ctx.pos++; // consume '('
                skipWhitespace(ctx);
                if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '"')
                    annotValue = readQuotedString(ctx);
                else
                {
                    // Unquoted value: read until ')'
                    annotValue = readUntil(ctx, ')');
                    // readUntil already consumed ')', so don't consume again
                    size_t ts = annotValue.find_first_not_of(" \t");
                    size_t te = annotValue.find_last_not_of(" \t\r");
                    if (ts != std::string::npos)
                        annotValue = annotValue.substr(ts, te - ts + 1);
                    else
                        annotValue.clear();
                    goto nextAnnotation; // '(' already consumed by readUntil above
                }
                skipWhitespace(ctx);
                if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == ')')
                    ctx.pos++;
            }

        nextAnnotation:
            std::string lower = annotName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower == "group")
                prop.group = annotValue;
            else if (lower == "tooltip")
                prop.tooltip = annotValue;
            else if (lower == "if")
                prop.ifKeyword = annotValue;
            // Unknown annotations are silently ignored for forward compat.
        }
        return true;
    }

    // Properties block
    //
    // Extended syntax on top of the original:
    //   @group("Name")        -- editor grouping
    //   @tooltip("Text")      -- inspector hover text
    //   @if(KEYWORD)          -- conditional visibility
    //
    //   New types (case-insensitive):
    //     Texture2D, Texture3D, TextureCube
    //     Range(min, max)      -- constrained float with bounds
    //     Enum(TypeName)       -- integer backed by a C++ enum
    //     Gradient             -- color ramp
    //
    // All existing syntax forms remain supported unchanged.
    // All new types fall back to stringProps for backward compat.
    bool ShaderParser::parsePropertiesBlock(ParseContext &ctx)
    {
        skipWhitespace(ctx);
        if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] != '{')
        {
            setError("Expected '{' after Properties");
            return false;
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

            // Skip comment lines
            if (ctx.source[ctx.pos] == '/' &&
                ctx.pos + 1 < ctx.source.length() && ctx.source[ctx.pos + 1] == '/')
            {
                readUntil(ctx, '\n');
                continue;
            }

            ShaderProp prop;
            parseAnnotations(ctx, prop);

            skipWhitespace(ctx);
            if (ctx.pos >= ctx.source.length() || ctx.source[ctx.pos] == '}')
                break;

            std::string propName = readToken(ctx);
            if (propName.empty())
            {
                ctx.pos++;
                continue;
            }

            skipWhitespace(ctx);

            std::string displayName;
            std::string typeStr;
            float rangeMin = 0.0f, rangeMax = 1.0f;
            std::string enumTypeName;

            if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '(')
            {
                ctx.pos++; // consume '('
                skipWhitespace(ctx);

                if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '"')
                {
                    displayName = readQuotedString(ctx);
                    skipWhitespace(ctx);
                    if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == ',')
                        ctx.pos++;
                    skipWhitespace(ctx);
                }

                typeStr = readToken(ctx);
                skipWhitespace(ctx);

                // Handle Range(min, max) and Enum(TypeName) sub-parens
                {
                    std::string typeStrLower = typeStr;
                    std::transform(typeStrLower.begin(), typeStrLower.end(),
                                   typeStrLower.begin(), ::tolower);

                    if ((typeStrLower == "range" || typeStrLower == "enum") &&
                        ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '(')
                    {
                        ctx.pos++; // consume inner '('
                        if (typeStrLower == "range")
                        {
                            skipWhitespace(ctx);
                            std::string minStr = readUntil(ctx, ',');
                            std::string maxStr = readUntil(ctx, ')');
                            // readUntil consumed ')'; trim and parse
                            auto trim = [](const std::string &s) -> std::string
                            {
                                size_t a = s.find_first_not_of(" \t");
                                size_t b = s.find_last_not_of(" \t\r");
                                return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
                            };
                            try
                            {
                                rangeMin = std::stof(trim(minStr));
                            }
                            catch (...)
                            {
                            }
                            try
                            {
                                rangeMax = std::stof(trim(maxStr));
                            }
                            catch (...)
                            {
                            }
                        }
                        else // Enum
                        {
                            skipWhitespace(ctx);
                            enumTypeName = readUntil(ctx, ')');
                            size_t ts = enumTypeName.find_first_not_of(" \t");
                            size_t te = enumTypeName.find_last_not_of(" \t\r");
                            if (ts != std::string::npos)
                                enumTypeName = enumTypeName.substr(ts, te - ts + 1);
                            else
                                enumTypeName.clear();
                        }
                        skipWhitespace(ctx);
                    }
                }

                skipWhitespace(ctx);
                if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == ')')
                    ctx.pos++;
            }

            skipWhitespace(ctx);

            // Optional: = defaultValue
            std::string rawDefault;
            if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '=')
            {
                ctx.pos++;
                skipWhitespace(ctx);

                if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '"')
                {
                    rawDefault = readQuotedString(ctx);
                }
                else if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '(')
                {
                    rawDefault = "(";
                    ctx.pos++;
                    rawDefault += readUntil(ctx, ')');
                    rawDefault += ")";
                }
                else if (ctx.pos < ctx.source.length() && ctx.source[ctx.pos] == '[')
                {
                    // Gradient literal: [ (t, (r,g,b,a)), ... ]
                    // Capture everything up to the matching ']'
                    rawDefault = "[";
                    ctx.pos++;
                    int depth = 1;
                    while (ctx.pos < ctx.source.length() && depth > 0)
                    {
                        char c = ctx.source[ctx.pos++];
                        if (c == '[')
                            depth++;
                        else if (c == ']')
                        {
                            depth--;
                            if (depth == 0)
                            {
                                rawDefault += ']';
                                break;
                            }
                        }
                        rawDefault += c;
                    }
                }
                else
                {
                    while (ctx.pos < ctx.source.length())
                    {
                        char c = ctx.source[ctx.pos];
                        if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == '}')
                            break;
                        rawDefault += c;
                        ctx.pos++;
                    }
                }
            }

            // Skip remainder of the line
            {
                size_t nlPos = ctx.source.find('\n', ctx.pos);
                if (nlPos != std::string::npos)
                    ctx.pos = nlPos;
            }

            if (propName.empty())
                continue;

            // Determine PropType

            std::string typeStrLower = typeStr;
            std::transform(typeStrLower.begin(), typeStrLower.end(),
                           typeStrLower.begin(), ::tolower);

            PropType propType = PropType::String;
            if (typeStrLower == "int" || typeStrLower == "integer")
                propType = PropType::Int;
            else if (typeStrLower == "float")
                propType = PropType::Float;
            else if (typeStrLower == "bool" || typeStrLower == "boolean")
                propType = PropType::Bool;
            else if (typeStrLower == "string" || typeStrLower == "text" || typeStrLower.empty())
                propType = PropType::String;
            else if (typeStrLower == "color" || typeStrLower == "colour")
                propType = PropType::Color;
            else if (typeStrLower == "vector")
                propType = PropType::Vector;
            else if (typeStrLower == "texture2d")
                propType = PropType::Texture2D;
            else if (typeStrLower == "texture3d")
                propType = PropType::Texture3D;
            else if (typeStrLower == "texturecube")
                propType = PropType::TextureCube;
            else if (typeStrLower == "range")
                propType = PropType::Range;
            else if (typeStrLower == "enum")
                propType = PropType::Enum;
            else if (typeStrLower == "gradient")
                propType = PropType::Gradient;

            // Build ShaderProp (metadata already partially filled by annotations)

            prop.name = propName;
            prop.displayName = displayName.empty() ? propName : displayName;
            prop.type = propType;
            prop.rawDefault = rawDefault;
            prop.rangeMin = rangeMin;
            prop.rangeMax = rangeMax;
            prop.enumTypeName = enumTypeName;
            ctx.shader->properties.push_back(prop);

            // Populate typed convenience maps (backward compat)

            try
            {
                switch (propType)
                {
                case PropType::Int:
                case PropType::Enum:
                    ctx.shader->intProps[propName] =
                        rawDefault.empty() ? 0 : std::stoi(rawDefault);
                    break;

                case PropType::Float:
                case PropType::Range:
                    ctx.shader->floatProps[propName] =
                        rawDefault.empty() ? 0.0f : std::stof(rawDefault);
                    break;

                case PropType::Bool:
                {
                    std::string bv = rawDefault;
                    std::transform(bv.begin(), bv.end(), bv.begin(), ::tolower);
                    ctx.shader->boolProps[propName] = (bv == "true" || bv == "1");
                    break;
                }

                case PropType::String:
                case PropType::Color:
                case PropType::Vector:
                case PropType::Texture2D:
                case PropType::Texture3D:
                case PropType::TextureCube:
                case PropType::Gradient:
                default:
                    ctx.shader->stringProps[propName] = rawDefault;
                    break;
                }
            }
            catch (const std::exception &)
            {
                ctx.shader->stringProps[propName] = rawDefault;
            }
        }

        return true;
    }

    // extractWorkgroupSize
    // Looks for:  layout(local_size_x = X, local_size_y = Y, local_size_z = Z) in;
    // Values default to 1 if a component is absent. Returns true if found.

    bool ShaderParser::extractWorkgroupSize(const std::string &source, glm::uvec3 &outSize)
    {
        static const std::string kAnchor = "local_size_x";
        size_t anchor = source.find(kAnchor);
        if (anchor == std::string::npos)
            return false;

        size_t layoutPos = source.rfind("layout", anchor);
        if (layoutPos == std::string::npos)
            return false;

        size_t parenOpen = source.find('(', layoutPos);
        size_t parenClose = source.find(')', parenOpen);
        if (parenOpen == std::string::npos || parenClose == std::string::npos)
            return false;

        std::string inner = source.substr(parenOpen + 1, parenClose - parenOpen - 1);

        auto parseComp = [&](const std::string &key, uint32_t &out)
        {
            size_t p = inner.find(key);
            if (p == std::string::npos)
                return;
            size_t eq = inner.find('=', p + key.size());
            if (eq == std::string::npos)
                return;
            size_t numStart = inner.find_first_not_of(" \t", eq + 1);
            if (numStart == std::string::npos)
                return;
            try
            {
                out = static_cast<uint32_t>(std::stoul(inner.substr(numStart)));
            }
            catch (...)
            {
            }
        };

        outSize = {1, 1, 1};
        parseComp("local_size_x", outSize.x);
        parseComp("local_size_y", outSize.y);
        parseComp("local_size_z", outSize.z);
        return true;
    }

    // extractEntryPoint
    // Looks for:  #pragma entry <name>  anywhere in source.

    std::optional<std::string> ShaderParser::extractEntryPoint(const std::string &source)
    {
        std::istringstream ss(source);
        std::string line;
        while (std::getline(ss, line))
        {
            size_t f = line.find_first_not_of(" \t");
            if (f == std::string::npos || line[f] != '#')
                continue;
            size_t p = f + 1;
            while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
                p++;
            if (line.substr(p, 6) != "pragma")
                continue;
            p += 6;
            while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
                p++;
            if (line.substr(p, 5) != "entry")
                continue;
            p += 5;
            while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
                p++;
            std::string name;
            while (p < line.size() && !std::isspace(static_cast<unsigned char>(line[p])))
                name += line[p++];
            if (!name.empty())
                return name;
        }
        return std::nullopt;
    }

    // Preprocessing

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

        // Emit SPIR-V specialization constant declarations from #pragma specialize
        for (const auto &sc : shader.specializationConstants)
        {
            result += "layout(constant_id = " + std::to_string(sc.constantId) +
                      ") const int " + sc.name + " = " +
                      (sc.defaultValue.empty() ? "0" : sc.defaultValue) + ";\n";
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

        // Normalise #version: strip all occurrences and re-add at the top
        {
            size_t vpos = stageBody.find("#version");
            while (vpos != std::string::npos)
            {
                size_t end = stageBody.find('\n', vpos);
                stageBody.erase(vpos, end == std::string::npos
                                          ? std::string::npos
                                          : end - vpos + 1);
                vpos = stageBody.find("#version");
            }
            stageBody = "#version 450 core\n" + stageBody;
        }

        ShaderIncludeResolver resolver;
        if (!shader.filepath.empty())
            resolver.addIncludeDirectory(ShaderIncludeUtils::getDirectory(shader.filepath));

        resolver.addIncludeDirectory(".");
        resolver.addIncludeDirectory("./Shaders");
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
            // Fall back: strip #import lines but keep everything else
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

    // Compilation

    std::vector<uint32_t> ShaderParser::compileGLSL(const std::string &source,
                                                    ShaderStage stage,
                                                    const std::string &entryPoint)
    {
        EShLanguage glslStage = ToGlslangStage(stage);
        glslang::TShader glShader(glslStage);
        const char *src = source.c_str();
        glShader.setStrings(&src, 1);
        glShader.setEnvInput(glslang::EShSourceGlsl, glslStage, glslang::EShClientVulkan, 450);
        glShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        glShader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        const TBuiltInResource *res = GetDefaultResources();
        EShMessages msg = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
        DirStackFileIncluder includer;
        includer.pushExternalLocalDirectory(".");
        includer.pushExternalLocalDirectory("./Shaders");
        includer.pushExternalLocalDirectory("./Shaders/Clouds");
        includer.pushExternalLocalDirectory("./Shaders/Common");
        includer.pushExternalLocalDirectory("./Shaders/Atmosphere");
        includer.pushExternalLocalDirectory("./Shaders/Lighting");
        std::string preprocessed;

        if (!glShader.preprocess(res, 450, ECoreProfile, false, false, msg, &preprocessed, includer))
        {
            setError("GLSL preprocess failed:\n" + std::string(glShader.getInfoLog()));
            return {};
        }
        const char *pp = preprocessed.c_str();
        glShader.setStrings(&pp, 1);
        if (!glShader.parse(res, 450, ECoreProfile, false, false, msg, includer))
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

        const char *ep = entryPoint.empty() ? "main" : entryPoint.c_str();
        glShader.setEntryPoint(ep);
        glShader.setSourceEntryPoint(ep);

        glShader.setEnvInput(glslang::EShSourceHlsl, glslStage, glslang::EShClientVulkan, 0);
        glShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
        glShader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

        const TBuiltInResource *res = GetDefaultResources();
        EShMessages msg = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);
        DirStackFileIncluder includer;
        includer.pushExternalLocalDirectory(".");
        includer.pushExternalLocalDirectory("./Shaders");
        includer.pushExternalLocalDirectory("./Shaders/Clouds");
        includer.pushExternalLocalDirectory("./Shaders/Common");
        includer.pushExternalLocalDirectory("./Shaders/Atmosphere");
        includer.pushExternalLocalDirectory("./Shaders/Lighting");
        std::string preprocessed;

        if (!glShader.preprocess(res, 0, ENoProfile, false, false, msg, &preprocessed, includer))
        {
            setError("HLSL preprocess failed:\n" + std::string(glShader.getInfoLog()));
            return {};
        }
        const char *pp = preprocessed.c_str();
        glShader.setStrings(&pp, 1);
        if (!glShader.parse(res, 0, ENoProfile, false, false, msg, includer))
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
