#include "Parser.hpp"

#include <slang.h>
#include <slang-com-ptr.h>
#include <volk.h>

#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace SF::Engine::Shaders
{
    static SlangStage ToSlangStage(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return SLANG_STAGE_VERTEX;
        case ShaderStage::Fragment:
            return SLANG_STAGE_FRAGMENT;
        case ShaderStage::Compute:
            return SLANG_STAGE_COMPUTE;
        case ShaderStage::Geometry:
            return SLANG_STAGE_GEOMETRY;
        case ShaderStage::TessellationControl:
            return SLANG_STAGE_HULL;
        case ShaderStage::TessellationEvaluation:
            return SLANG_STAGE_DOMAIN;
        default:
            return SLANG_STAGE_VERTEX;
        }
    }

    static std::optional<ShaderStage> FromSlangStage(SlangStage stage)
    {
        switch (stage)
        {
        case SLANG_STAGE_VERTEX:
            return ShaderStage::Vertex;
        case SLANG_STAGE_FRAGMENT:
            return ShaderStage::Fragment;
        case SLANG_STAGE_COMPUTE:
            return ShaderStage::Compute;
        case SLANG_STAGE_GEOMETRY:
            return ShaderStage::Geometry;
        case SLANG_STAGE_HULL:
            return ShaderStage::TessellationControl;
        case SLANG_STAGE_DOMAIN:
            return ShaderStage::TessellationEvaluation;
        default:
            return std::nullopt; // e.g. SLANG_STAGE_NONE -- not one of ours
        }
    }

    // loadModuleFromSourceString has no explicit "source language" argument --
    // Slang infers the frontend from the extension on `path`. There's no real
    // file behind it, so fabricate one that matches ShaderLanguage.
    static const char *FakeModulePathForLanguage(ShaderLanguage lang)
    {
        switch (lang)
        {
        case ShaderLanguage::GLSL:
            return "shader_module.glsl";
        case ShaderLanguage::HLSL:
            return "shader_module.hlsl";
        case ShaderLanguage::SLANG:
        default:
            return "shader_module.slang";
        }
    }

    // Process-lifetime global session, created lazily once.
    static slang::IGlobalSession *GetSlangGlobalSession()
    {
        static Slang::ComPtr<slang::IGlobalSession> session = []
        {
            Slang::ComPtr<slang::IGlobalSession> s;
            slang::createGlobalSession(s.writeRef());
            return s;
        }();
        return session.get();
    }

    namespace
    {
        // Everything below is an implementation detail kept out of Parser.hpp
        // so the header doesn't have to drag <slang.h> around.

        struct SlangModuleHandle
        {
            Slang::ComPtr<slang::ISession> session;
            slang::IModule *module = nullptr; // owned by session
        };

        std::string DrainDiagnostics(slang::IBlob *diagnostics)
        {
            if (diagnostics && diagnostics->getBufferSize() > 0)
                return std::string(static_cast<const char *>(diagnostics->getBufferPointer()),
                                   diagnostics->getBufferSize());
            return {};
        }

        // Creates a fresh Slang session targeting SPIR-V directly and loads
        // `resolvedSource` as a module. Returns an empty optional + `outError`
        // set on failure.
        std::optional<SlangModuleHandle> LoadSlangModule(const ParsedShader &shader,
                                                          const std::string &resolvedSource,
                                                          std::string &outError)
        {
            slang::IGlobalSession *globalSession = GetSlangGlobalSession();
            if (!globalSession)
            {
                outError = "Failed to create Slang global session";
                return std::nullopt;
            }

            slang::TargetDesc targetDesc = {};
            targetDesc.format = SLANG_SPIRV;
            targetDesc.profile = globalSession->findProfile("spirv_1_5");
            targetDesc.flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY;

            static const std::vector<std::string> kSearchDirs = {
                ".", "./Shaders", "./Shaders/Clouds", "./Shaders/Common",
                "./Shaders/Atmosphere", "./Shaders/Lighting"};
            std::vector<const char *> searchPaths;
            searchPaths.reserve(kSearchDirs.size());
            for (const auto &d : kSearchDirs)
                searchPaths.push_back(d.c_str());

            slang::SessionDesc sessionDesc = {};
            sessionDesc.targets = &targetDesc;
            sessionDesc.targetCount = 1;
            sessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
            sessionDesc.searchPaths = searchPaths.data();
            sessionDesc.searchPathCount = static_cast<SlangInt>(searchPaths.size());

            SlangModuleHandle handle;
            if (SLANG_FAILED(globalSession->createSession(sessionDesc, handle.session.writeRef())))
            {
                outError = "Failed to create Slang session";
                return std::nullopt;
            }

            Slang::ComPtr<slang::IBlob> diagnostics;
            handle.module = handle.session->loadModuleFromSourceString(
                "shader_module", FakeModulePathForLanguage(shader.language),
                resolvedSource.c_str(), diagnostics.writeRef());

            std::string diagText = DrainDiagnostics(diagnostics);
            if (!diagText.empty())
                std::cerr << "Slang: " << diagText << std::endl;

            if (!handle.module)
            {
                outError = "Slang module compilation failed for '" + shader.name + "'" +
                           (diagText.empty() ? "" : ": " + diagText);
                return std::nullopt;
            }
            return handle;
        }

        // Composes + links + generates SPIR-V for a single already-discovered
        // entry point out of an already-loaded module.
        std::vector<uint32_t> CompileEntryPointFromModule(slang::ISession *session, slang::IModule *module,
                                                           const std::string &entryName, ShaderStage stage,
                                                           std::string &outError)
        {
            Slang::ComPtr<slang::IEntryPoint> entry;
            Slang::ComPtr<slang::IBlob> diagnostics;
            SlangResult epResult = module->findAndCheckEntryPoint(
                entryName.c_str(), ToSlangStage(stage), entry.writeRef(), diagnostics.writeRef());

            std::string diagText = DrainDiagnostics(diagnostics);
            if (!diagText.empty())
                std::cerr << "Slang: " << diagText << std::endl;

            if (SLANG_FAILED(epResult) || !entry)
            {
                outError = "Slang: entry point '" + entryName + "' not found for stage " +
                           std::string(stageToString(stage));
                return {};
            }

            slang::IComponentType *components[] = {module, entry.get()};
            Slang::ComPtr<slang::IComponentType> program;
            diagnostics = nullptr;
            if (SLANG_FAILED(session->createCompositeComponentType(
                    components, 2, program.writeRef(), diagnostics.writeRef())))
            {
                outError = "Slang: failed to compose program: " + DrainDiagnostics(diagnostics);
                return {};
            }

            Slang::ComPtr<slang::IComponentType> linkedProgram;
            diagnostics = nullptr;
            if (SLANG_FAILED(program->link(linkedProgram.writeRef(), diagnostics.writeRef())))
            {
                outError = "Slang: link failed: " + DrainDiagnostics(diagnostics);
                return {};
            }

            Slang::ComPtr<slang::IBlob> spirvBlob;
            diagnostics = nullptr;
            if (SLANG_FAILED(linkedProgram->getEntryPointCode(
                    0, 0, spirvBlob.writeRef(), diagnostics.writeRef())))
            {
                outError = "Slang: codegen failed: " + DrainDiagnostics(diagnostics);
                return {};
            }

            if (!spirvBlob || spirvBlob->getBufferSize() == 0)
            {
                outError = "Slang produced empty SPIR-V for entry point: " + entryName;
                return {};
            }

            size_t byteSize = spirvBlob->getBufferSize();
            std::vector<uint32_t> spirv(byteSize / sizeof(uint32_t));
            std::memcpy(spirv.data(), spirvBlob->getBufferPointer(), byteSize);
            return spirv;
        }

        std::string DeriveShaderName(const std::string &pathOrName)
        {
            if (pathOrName.empty())
                return "unnamed";
            std::filesystem::path p(pathOrName);
            return p.has_stem() ? p.stem().string() : pathOrName;
        }
    } // namespace

    // ShaderParser

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
        {
            result->filepath = filepath;

            // Infer language from real extension unless a #pragma already overrode it.
            std::string ext = std::filesystem::path(filepath).extension().string();
            if (ext == ".slang" || ext == ".shader" || ext == ".si")
                result->language = ShaderLanguage::SLANG;
            else if (ext == ".hlsl")
                result->language = ShaderLanguage::HLSL;
            // else leave as GLSL default
        }
        return result;
    }

    std::optional<ParsedShader> ShaderParser::parseSource(const std::string &source,
                                                          const std::string &name)
    {
        ParsedShader shader;
        shader.name = DeriveShaderName(name);
        shader.language = ShaderLanguage::GLSL;
        shader.source = source;

        scanPragmas(shader);

        return shader;
    }

    // Compiles once with no extra defines purely to ask Slang what entry
    // points the file defines. compile()/compileAll() call this lazily if
    // shader.entryPoints is still empty.
    bool ShaderParser::discoverEntryPoints(ParsedShader &shader)
    {
        std::string resolved = preprocess(shader, {});

        std::string error;
        auto handle = LoadSlangModule(shader, resolved, error);
        if (!handle)
        {
            setError(error);
            return false;
        }

        shader.entryPoints.clear();

        SlangInt32 count = handle->module->getDefinedEntryPointCount();
        for (SlangInt32 i = 0; i < count; i++)
        {
            Slang::ComPtr<slang::IEntryPoint> entry;
            if (SLANG_FAILED(handle->module->getDefinedEntryPoint(i, entry.writeRef())) || !entry)
                continue;

            Slang::ComPtr<slang::IBlob> diagnostics;
            slang::ProgramLayout *layout = entry->getLayout(0, diagnostics.writeRef());
            if (!layout || layout->getEntryPointCount() == 0)
                continue;

            slang::EntryPointReflection *epRefl = layout->getEntryPointByIndex(0);
            auto stageOpt = FromSlangStage(epRefl->getStage());
            if (!stageOpt)
                continue; // not a real pipeline stage (e.g. plain helper function)

            ShaderEntryPointInfo info;
            info.name = epRefl->getName();
            info.stage = *stageOpt;

            if (info.stage == ShaderStage::Compute)
            {
                SlangUInt sizes[3] = {1, 1, 1};
                epRefl->getComputeThreadGroupSize(3, sizes);
                info.workgroupSize = {static_cast<uint32_t>(sizes[0]),
                                      static_cast<uint32_t>(sizes[1]),
                                      static_cast<uint32_t>(sizes[2])};
                info.hasWorkgroupSize = true;

                if (shader.hasWorkgroupSizeOverride)
                    info.workgroupSize = shader.workgroupSizeOverride;
            }

            shader.entryPoints.push_back(std::move(info));
        }

        if (shader.entryPoints.empty())
        {
            setError("Slang: no [shader(\"...\")] entry points found in '" + shader.name + "'");
            return false;
        }
        return true;
    }

    std::optional<std::vector<CompiledShader>> ShaderParser::compileAll(
        const ParsedShader &shader, const std::vector<SF::Engine::Shader::Define> &defines)
    {
        ParsedShader working = shader;
        if (working.entryPoints.empty() && !discoverEntryPoints(working))
            return std::nullopt;

        std::string resolved = preprocess(working, defines);

        std::string error;
        auto handle = LoadSlangModule(working, resolved, error);
        if (!handle)
        {
            setError(error);
            return std::nullopt;
        }

        std::vector<CompiledShader> results;
        results.reserve(working.entryPoints.size());

        for (const auto &ep : working.entryPoints)
        {
            std::string compileError;
            auto spirv = CompileEntryPointFromModule(handle->session.get(), handle->module,
                                                      ep.name, ep.stage, compileError);
            if (spirv.empty())
            {
                setError(compileError);
                std::cerr << "ShaderParser: empty SPIRV for entry '" << ep.name << "' in '"
                          << working.name << "'. Error: " << lastError_ << std::endl;
                return std::nullopt;
            }

            CompiledShader compiled;
            compiled.name = working.name;
            compiled.stage = ep.stage;
            compiled.language = working.language;
            compiled.spirv = std::move(spirv);
            compiled.entryPoint = ep.name;
            compiled.workgroupSize = ep.workgroupSize;
            compiled.hasWorkgroupSize = ep.hasWorkgroupSize;
            results.push_back(std::move(compiled));
        }

        return results;
    }

    std::optional<CompiledShader> ShaderParser::compile(
        const ParsedShader &shader, ShaderStage stage,
        const std::vector<SF::Engine::Shader::Define> &defines)
    {
        // Note: this recompiles every entry point in the file to grab one --
        // fine for now, but if you're calling this per-stage in a hot loop,
        // switch to compileAll() and pick the stage out of the result instead.
        auto all = compileAll(shader, defines);
        if (!all)
            return std::nullopt;

        for (auto &c : *all)
            if (c.stage == stage)
                return std::move(c);

        setError("Stage not found in shader: " + std::string(stageToString(stage)));
        return std::nullopt;
    }

    // #pragma scan
    //
    // No block structure left to walk -- just a flat line scan for directives
    // we care about. Everything else in the file is left untouched for Slang.
    //
    //   hlsl / slang                   -- pick the Slang frontend
    //   workgroup_size <x> [y] [z]     -- override the compute entry's group size
    //   multi_compile[_local] <KW...>  -- register a multi-compile keyword set
    //   require <feature> [tier]       -- add capability requirement
    //   fallback "path"                -- set fallback shader path
    //   specialize NAME = default      -- declare SPIR-V specialization constant
    //   anything else                  -- ignored (Slang no-ops unknown pragmas too)

    void ShaderParser::scanPragmas(ParsedShader &shader)
    {
        std::istringstream ss(shader.source);
        std::string line;

        while (std::getline(ss, line))
        {
            size_t f = line.find_first_not_of(" \t");
            if (f == std::string::npos || line[f] != '#')
                continue;

            size_t p = f + 1;
            while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
                p++;
            if (line.compare(p, 6, "pragma") != 0)
                continue;
            p += 6;
            while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p])))
                p++;

            std::string rest = line.substr(p);

            std::vector<std::string> tokens;
            {
                std::istringstream ls(rest);
                std::string t;
                while (ls >> t)
                    tokens.push_back(t);
            }
            if (tokens.empty())
                continue;

            const std::string &directive = tokens[0];

            if (directive == "hlsl")
            {
                shader.language = ShaderLanguage::HLSL;
            }
            else if (directive == "slang")
            {
                shader.language = ShaderLanguage::SLANG;
            }
            else if (directive == "workgroup_size" && tokens.size() >= 2)
            {
                shader.workgroupSizeOverride.x = static_cast<uint32_t>(std::stoul(tokens[1]));
                shader.workgroupSizeOverride.y = tokens.size() >= 3
                                                     ? static_cast<uint32_t>(std::stoul(tokens[2]))
                                                     : 1u;
                shader.workgroupSizeOverride.z = tokens.size() >= 4
                                                     ? static_cast<uint32_t>(std::stoul(tokens[3]))
                                                     : 1u;
                shader.hasWorkgroupSizeOverride = true;
            }
            else if ((directive == "multi_compile" || directive == "multi_compile_local") &&
                    tokens.size() >= 2)
            {
                shader.multiCompileKeywords.emplace_back(tokens.begin() + 1, tokens.end());
            }
            else if (directive == "require" && tokens.size() >= 2)
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
                shader.requirements.push_back(req);
            }
            else if (directive == "fallback")
            {
                size_t q = rest.find('"');
                if (q != std::string::npos)
                {
                    size_t q2 = rest.find('"', q + 1);
                    if (q2 != std::string::npos)
                        shader.fallbackShader = rest.substr(q + 1, q2 - q - 1);
                }
                else if (tokens.size() >= 2)
                {
                    shader.fallbackShader = tokens[1];
                }
            }
            else if (directive == "specialize" && tokens.size() >= 2)
            {
                ShaderSpecializationConstant sc;
                sc.constantId = static_cast<uint32_t>(shader.specializationConstants.size());

                std::string nameEqVal = tokens[1];
                size_t eq = nameEqVal.find('=');
                if (eq != std::string::npos)
                {
                    sc.name = nameEqVal.substr(0, eq);
                    sc.defaultValue = nameEqVal.substr(eq + 1);
                    while (!sc.defaultValue.empty() && std::isspace(sc.defaultValue.front()))
                        sc.defaultValue.erase(sc.defaultValue.begin());
                }
                else
                {
                    sc.name = nameEqVal;
                    size_t valIdx = (tokens.size() > 2 && tokens[2] == "=") ? 3 : 2;
                    if (valIdx < tokens.size())
                        sc.defaultValue = tokens[valIdx];
                }

                if (!sc.name.empty())
                    shader.specializationConstants.push_back(sc);
            }
            // else: unknown pragma -- ignored for forward compatibility
        }
    }

    // Preprocessing
    //
    // Whole-file now -- there's no per-stage substring to splice defines into,
    // since stages are just ordinary functions tagged [shader("...")] living
    // in the same scope. Caller-supplied defines and #pragma specialize decls
    // apply to the entire compiled module.

    std::string ShaderParser::preprocess(const ParsedShader &shader,
                                         const std::vector<SF::Engine::Shader::Define> &defines)
    {
        std::string result;

        for (const auto &[name, value] : defines)
        {
            result += "#define " + name;
            if (!value.empty())
                result += " " + value;
            result += "\n";
        }

        for (const auto &sc : shader.specializationConstants)
        {
            result += "layout(constant_id = " + std::to_string(sc.constantId) +
                      ") const int " + sc.name + " = " +
                      (sc.defaultValue.empty() ? "0" : sc.defaultValue) + ";\n";
        }

        std::string body = result + shader.source;

        // #version is GLSL-only; strip whatever the author left in and re-add
        // ours at the top. Left alone for HLSL/Slang-tagged shaders.
        if (shader.language == ShaderLanguage::GLSL)
        {
            size_t vpos = body.find("#version");
            while (vpos != std::string::npos)
            {
                size_t end = body.find('\n', vpos);
                body.erase(vpos, end == std::string::npos ? std::string::npos : end - vpos + 1);
                vpos = body.find("#version");
            }
            body = "#version 450 core\n" + body;
        }

        std::string basePath = "./Shaders";

        return body;
    }

} // namespace SF::Engine::Shaders