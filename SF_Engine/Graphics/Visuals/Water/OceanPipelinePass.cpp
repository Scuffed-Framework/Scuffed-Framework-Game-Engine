#define VK_NO_PROTOTYPES

#define SF_OCEAN_DEBUG_LOG

#include "OceanPipelinePass.hpp"
#include <Graphics/Mesh/Vertex.hpp> // PatchVertex::GetVertexInput()
#include <Graphics/RenderSystem.hpp>
#include <Graphics/SharedFunctions.hpp>
#include <Graphics/Shaders/Parser/Parser.hpp>

namespace SF::Engine
{
    OceanTessellationPipelinePass::OceanTessellationPipelinePass(
        Pipeline::Stage stage,
        const OceanTessellationParams &params)
        : PipelinePass(stage), params_(params)
    {
        clipmapMesh_ = std::make_unique<OceanClipmapMesh>(
            /*ringCount=*/5,
            /*baseExtent=*/200.0f,
            /*patchCount=*/params_.patchCount);

        ubo_ = std::make_unique<UniformBuffer>(sizeof(OceanTessellationFrameUBO));
        spectrumUBO_ = std::make_unique<UniformBuffer>(sizeof(OceanFFTSpectrumUBO));

        const uint32_t N = fftSettings_.N;
        const UVec2 size{N, N};

        initialSpectrumTex_ = std::make_unique<Image2dArray>(
            size,
            4,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            false,
            false);

        spectrumTex_ = std::make_unique<Image2dArray>(
            size,
            8,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            false,
            false);

        fourierTarget_ = std::make_unique<Image2dArray>(
            size,
            8,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            false,
            false);

        displacementTex_ = std::make_unique<Image2dArray>(
            size,
            4,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            false,
            true);

        slopeTex_ = std::make_unique<Image2dArray>(
            size,
            4,
            VK_FORMAT_R32G32_SFLOAT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_REPEAT,
            false,
            true);

        spectrumParamsBuf_ = std::make_unique<Buffer>(
            sizeof(SpectrumParameters) * fftSettings_.spectrums.size(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            std::as_bytes(std::span(fftSettings_.spectrums)));

        // ---- Compute pipelines: one per kernel. Each #pragma kernel in
        // OceanFFT.comp.shader becomes its own entry point selected via a
        // define, since ComputePipeline compiles a single shader file with
        // a single entry point. ----
        auto kernelDefines = [](const char *kernelName)
        {
            return std::vector<Shader::Define>{{"OCEAN_FFT_KERNEL", kernelName}};
        };

        initSpectrumPipeline_ /*****/ = std::make_unique<ComputePipeline>("Shaders/Ocean/OceanFFTSpectrum.shader", kernelDefines("CS_InitializeSpectrum"));
        packConjugatePipeline_ /****/ = std::make_unique<ComputePipeline>("Shaders/Ocean/OceanFFTSpectrum.shader", kernelDefines("CS_PackSpectrumConjugate"));
        updateSpectrumPipeline_ /* */ = std::make_unique<ComputePipeline>("Shaders/Ocean/OceanFFTSpectrum.shader", kernelDefines("CS_UpdateSpectrumForFFT"));
        horizontalFFTPipeline_ /****/ = std::make_unique<ComputePipeline>("Shaders/Ocean/OceanFFTSpectrum.shader", kernelDefines("CS_HorizontalFFT"));
        verticalFFTPipeline_ /******/ = std::make_unique<ComputePipeline>("Shaders/Ocean/OceanFFTSpectrum.shader", kernelDefines("CS_VerticalFFT"));
        assemblePipeline_ /*********/ = std::make_unique<ComputePipeline>("Shaders/Ocean/OceanFFTSpectrum.shader", kernelDefines("CS_AssembleMaps"));

        std::vector<Shader::Define> defines =
            {
                {"USE_TESSELLATION", "1"},
                {"MAX_TESS_FACTOR", std::to_string(static_cast<int>(params_.tessFactor))},
            };

        // PatchVertex layout: location 0 = position, 1 = uv, 2 = normal.
        // RenderPipeline must configure VkPipelineTessellationStateCreateInfo
        // with patchControlPoints = 4 when topology == PATCH_LIST.
        pipeline_ = std::make_unique<RenderPipeline>(
            stage,
            "Shaders/Ocean/OceanTessellation.shader",
            std::vector<Shader::VertexInput>{PatchVertex::GetVertexInput()},
            defines,
            RenderPipeline::Mode::Polygon,
            RenderPipeline::Depth::ReadWrite,
            VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
            VK_POLYGON_MODE_FILL,
            VK_CULL_MODE_FRONT_BIT,
            VK_FRONT_FACE_COUNTER_CLOCKWISE);

        setupDescriptorSet();
        setupComputeDescriptorSets();

        {
            CommandBuffer initCmd = CommandBuffer(true);

            initSpectrumPipeline_->CmdRender(initCmd, {N / 8, N / 8});

            // CS_PackSpectrumConjugate reads the same image it writes for the
            // mirrored coordinate, so a barrier is needed between the two
            // dispatches even though they're both "init" passes.
            ImageArrayBarrier(initCmd, initialSpectrumTex_->GetImage(),
                              VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              /*layerCount=*/4);

            packConjugatePipeline_->CmdRender(initCmd, {N / 8, N / 8});

            // Leave initialSpectrumTex_ readable by CS_UpdateSpectrumForFFT
            // every frame thereafter.
            ImageArrayBarrier(initCmd, initialSpectrumTex_->GetImage(),
                              VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_ACCESS_SHADER_WRITE_BIT,
                              VK_ACCESS_SHADER_READ_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              /*layerCount=*/4);

            initCmd.SubmitIdle();
        }
    }

    void OceanTessellationPipelinePass::RebuildMesh()
    {
        clipmapMesh_ = std::make_unique<OceanClipmapMesh>(
            /*ringCount=*/5,
            /*baseExtent=*/200.0f,
            /*patchCount=*/params_.patchCount);
    }

    void OceanTessellationPipelinePass::setupDescriptorSet()
    {
        descSet_ = std::make_unique<DescriptorSet>(*pipeline_);

        VkDescriptorBufferInfo bufInfo{ubo_->GetBuffer(), 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet uboWrite{};
        uboWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uboWrite.dstSet = descSet_->GetDescriptorSet();
        uboWrite.dstBinding = 0;
        uboWrite.descriptorCount = 1;
        uboWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboWrite.pBufferInfo = &bufInfo;

        VkDescriptorImageInfo dispInfo{
            displacementTex_->GetSampler(),
            displacementTex_->GetView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        VkWriteDescriptorSet dispWrite{};
        dispWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        dispWrite.dstSet = descSet_->GetDescriptorSet();
        dispWrite.dstBinding = 1;
        dispWrite.descriptorCount = 1;
        dispWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        dispWrite.pImageInfo = &dispInfo;

        VkDescriptorImageInfo slopeInfo{
            slopeTex_->GetSampler(),
            slopeTex_->GetView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        VkWriteDescriptorSet slopeWrite{};
        slopeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        slopeWrite.dstSet = descSet_->GetDescriptorSet();
        slopeWrite.dstBinding = 2;
        slopeWrite.descriptorCount = 1;
        slopeWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        slopeWrite.pImageInfo = &slopeInfo;

        DescriptorSet::Update({uboWrite, dispWrite, slopeWrite});
    }

    void OceanTessellationPipelinePass::setupComputeDescriptorSets()
    {
        VkDescriptorBufferInfo spectrumBufInfo{spectrumUBO_->GetBuffer(), 0, VK_WHOLE_SIZE};
        VkDescriptorBufferInfo paramsBufInfo{spectrumParamsBuf_->GetBuffer(), 0, VK_WHOLE_SIZE};

        VkDescriptorImageInfo initialSpecInfo{VK_NULL_HANDLE, initialSpectrumTex_->GetView(), VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo spectrumInfo{VK_NULL_HANDLE, spectrumTex_->GetView(), VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo fourierInfo{VK_NULL_HANDLE, fourierTarget_->GetView(), VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo dispStorageInfo{VK_NULL_HANDLE, displacementTex_->GetView(), VK_IMAGE_LAYOUT_GENERAL};
        VkDescriptorImageInfo slopeStorageInfo{VK_NULL_HANDLE, slopeTex_->GetView(), VK_IMAGE_LAYOUT_GENERAL};

        // Helper lambdas, each captures the ds by value after the set is created.
        auto W_UBO = [&](VkDescriptorSet ds)
        {
            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = ds;
            w.dstBinding = 0;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.pBufferInfo = &spectrumBufInfo;
            return w;
        };
        auto W_Params = [&](VkDescriptorSet ds)
        {
            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = ds;
            w.dstBinding = 1;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            w.pBufferInfo = &paramsBufInfo;
            return w;
        };
        auto W_InitSpec = [&](VkDescriptorSet ds)
        {
            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = ds;
            w.dstBinding = 2;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.pImageInfo = &initialSpecInfo;
            return w;
        };
        auto W_Spectrum = [&](VkDescriptorSet ds)
        {
            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = ds;
            w.dstBinding = 3;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.pImageInfo = &spectrumInfo;
            return w;
        };
        auto W_Disp = [&](VkDescriptorSet ds)
        {
            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = ds;
            w.dstBinding = 4;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.pImageInfo = &dispStorageInfo;
            return w;
        };
        auto W_Slope = [&](VkDescriptorSet ds)
        {
            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = ds;
            w.dstBinding = 5;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.pImageInfo = &slopeStorageInfo;
            return w;
        };
        auto W_Fourier = [&](VkDescriptorSet ds)
        {
            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = ds;
            w.dstBinding = 6;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            w.pImageInfo = &fourierInfo;
            return w;
        };

        // CS_InitializeSpectrum, bindings 0, 1, 2
        {
            initSpectrumDescSet_ = std::make_unique<DescriptorSet>(*initSpectrumPipeline_);
            VkDescriptorSet ds = initSpectrumDescSet_->GetDescriptorSet();
            DescriptorSet::Update({W_UBO(ds), W_Params(ds), W_InitSpec(ds)});
        }
        // CS_PackSpectrumConjugate, bindings 0, 2
        {
            packConjugateDescSet_ = std::make_unique<DescriptorSet>(*packConjugatePipeline_);
            VkDescriptorSet ds = packConjugateDescSet_->GetDescriptorSet();
            DescriptorSet::Update({W_UBO(ds), W_InitSpec(ds)});
        }
        // CS_UpdateSpectrumForFFT, bindings 0, 2 (read), 3 (write)
        {
            updateSpectrumDescSet_ = std::make_unique<DescriptorSet>(*updateSpectrumPipeline_);
            VkDescriptorSet ds = updateSpectrumDescSet_->GetDescriptorSet();
            DescriptorSet::Update({W_UBO(ds), W_InitSpec(ds), W_Spectrum(ds)});
        }
        // CS_HorizontalFFT, bindings 0, 6
        {
            horizontalFFTDescSet_ = std::make_unique<DescriptorSet>(*horizontalFFTPipeline_);
            VkDescriptorSet ds = horizontalFFTDescSet_->GetDescriptorSet();
            DescriptorSet::Update({W_UBO(ds), W_Fourier(ds)});
        }
        // CS_VerticalFFT, bindings 0, 6
        {
            verticalFFTDescSet_ = std::make_unique<DescriptorSet>(*verticalFFTPipeline_);
            VkDescriptorSet ds = verticalFFTDescSet_->GetDescriptorSet();
            DescriptorSet::Update({W_UBO(ds), W_Fourier(ds)});
        }
        // CS_AssembleMaps, bindings 0, 3 (read), 4, 5
        {
            assembleDescSet_ = std::make_unique<DescriptorSet>(*assemblePipeline_);
            VkDescriptorSet ds = assembleDescSet_->GetDescriptorSet();
            DescriptorSet::Update({W_UBO(ds), W_Spectrum(ds), W_Disp(ds), W_Slope(ds)});
        }
    }

    void OceanTessellationPipelinePass::SetParams(const OceanTessellationParams &params)
    {
        const bool meshChanged = (params.patchCount != params_.patchCount) ||
                                 (params.patchExtent != params_.patchExtent);

        params_ = params;
        syncParamsToFrameData();

        if (meshChanged)
        {
            RebuildMesh();
        }
    }

    void OceanTessellationPipelinePass::syncParamsToFrameData()
    {
        // Wave
        frameData_.waveAmplitude = params_.waveAmplitude;
        frameData_.waveFrequency = params_.waveFrequency;
        frameData_.waveSpeed = params_.waveSpeed;
        frameData_.waveSteepness = params_.waveSteepness;
        frameData_.waveAmplitude2 = params_.waveAmplitude2;
        frameData_.waveFrequency2 = params_.waveFrequency2;
        frameData_.waveSpeed2 = params_.waveSpeed2;

        // Tessellation
        frameData_.tessFactor = params_.tessFactor;
        frameData_.minTessDistance = params_.minTessDistance;
        frameData_.maxTessDistance = params_.maxTessDistance;

        // Wind
        frameData_.windDirection = normalize(params_.windDirection);
        frameData_.timeScale = params_.timeScale;

        // Visual
        frameData_.oceanColor = params_.oceanColor;
        frameData_.glossiness = params_.glossiness;
        frameData_.shallowColor = params_.shallowColor;
        frameData_.specularPower = params_.specularPower;
        frameData_.foamColor = params_.foamColor;
        frameData_.foamIntensity = params_.foamIntensity;
        frameData_.foamThreshold = params_.foamThreshold;

        // Planet / sun
        frameData_.planetCenter = params_.planetCenter;
        frameData_.planetRadius = params_.planetRadius;
        frameData_.sunDirection = glm::length(params_.sunDirection) > 1e-6f
                                      ? normalize(params_.sunDirection)
                                      : Vec3(0.577f, 0.577f, 0.577f);

        // FFT cascade tiling (1 / lengthScale per cascade) and normal strength.
        frameData_.tile0 = 1.0f / fftSettings_.lengthScale0;
        frameData_.tile1 = 1.0f / fftSettings_.lengthScale1;
        frameData_.tile2 = 1.0f / fftSettings_.lengthScale2;
        frameData_.tile3 = 1.0f / fftSettings_.lengthScale3;
        frameData_.normalStrength = fftSettings_.normalStrength.x;
    }

    void OceanTessellationPipelinePass::RunFFTPass(const CommandBuffer &cmd)
    {
        const uint32_t N = fftSettings_.N;
        constexpr VkPipelineStageFlags kCompute = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        // 1. Update time-evolved spectrum (writes into fourierTarget_, which
        //    aliases spectrumTex_'s memory -- see CS_UpdateSpectrumForFFT
        //    binding setup: both bound to bindings 3/6 of the same images).
        updateSpectrumDescSet_->BindDescriptor(cmd);
        updateSpectrumPipeline_->CmdRender(cmd, {N / 8, N / 8});

        // Barrier: spectrum write -> FFT read/write (8 layers)
        ImageArrayBarrier(cmd, fourierTarget_->GetImage(),
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                          kCompute, kCompute, /*layerCount=*/8);

        // 2. 1024-point FFT, horizontal then vertical (in-place, log2(1024)=10
        //    butterfly passes each, done inside the compute shader's loop).
        horizontalFFTDescSet_->BindDescriptor(cmd);
        horizontalFFTPipeline_->CmdRender(cmd, {1, N});

        ImageArrayBarrier(cmd, fourierTarget_->GetImage(),
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                          kCompute, kCompute, /*layerCount=*/8);

        verticalFFTDescSet_->BindDescriptor(cmd);
        verticalFFTPipeline_->CmdRender(cmd, {1, N});

        ImageArrayBarrier(cmd, fourierTarget_->GetImage(),
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          kCompute, kCompute, /*layerCount=*/8);

        // 3. Assemble final displacement/slope/foam maps with permutation +
        //    Tessendorf choppiness + foam accumulation.
        assembleDescSet_->BindDescriptor(cmd);
        assemblePipeline_->CmdRender(cmd, {N / 8, N / 8});

        // Barrier: displacement/slope write -> graphics sampled read.
        ImageArrayBarrier(cmd, displacementTex_->GetImage(),
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          kCompute, VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT,
                          /*layerCount=*/4);

        ImageArrayBarrier(cmd, slopeTex_->GetImage(),
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          kCompute, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          /*layerCount=*/4);

        // Note: displacementTex_/slopeTex_ are left in SHADER_READ_ONLY_OPTIMAL
        // after the graphics passes consume them this frame. CS_AssembleMaps
        // (which reads back displacementTex_'s foam alpha next frame) needs
        // them back in GENERAL -- add a transition at the top of this
        // function (GENERAL <- SHADER_READ_ONLY_OPTIMAL) once the render
        // pass that samples them is known, so the layouts round-trip
        // correctly frame to frame.
    }

    void OceanTessellationPipelinePass::UpdateFrameData()
    {
        auto *cam = CameraController::Get().GetActive();
        auto *window = WindowManager::Get()->GetWindow(0);

        float dt = static_cast<float>(Engine::Get()->GetDelta().AsMilliseconds());
        accumulatedTime_ += dt;

        float aspect = window->GetAspectRatio();
        Mat4 view = cam->GetView();
        Mat4 proj = cam->GetProjection(aspect);

        frameData_.viewProj = proj * view;
        frameData_.invView = inverse(view);
        frameData_.invProj = inverse(proj);
        frameData_.cameraPos = cam->GetPosition();
        frameData_.time = accumulatedTime_;

        auto sz = window->GetSize();
        frameData_.screenSize = glm::vec2(sz.x, sz.y);

        syncParamsToFrameData();

        // Recenter the camera-relative ocean clipmap and project it onto the
        // planet sphere. planetCenter is vec3(0) in render coordinates
        // (frameData_.cameraPos is therefore already planet-relative).
        clipmapMesh_->RegenerateAt(frameData_.cameraPos,
                                   frameData_.planetCenter,
                                   frameData_.planetRadius);

#ifdef SF_OCEAN_DEBUG_LOG
        {
            static int s_logCount = 0;
            if (s_logCount < 5)
            {
                ++s_logCount;
                printf("[OceanDebug] === Frame %d ===\n", s_logCount);
                printf("[OceanDebug] cameraPos=(%.3f,%.3f,%.3f)\n",
                       frameData_.cameraPos.x, frameData_.cameraPos.y, frameData_.cameraPos.z);
                printf("[OceanDebug] planetCenter=(%.3f,%.3f,%.3f) planetRadius=%.3f\n",
                       frameData_.planetCenter.x, frameData_.planetCenter.y, frameData_.planetCenter.z,
                       frameData_.planetRadius);

                Vec3 up = frameData_.cameraPos - frameData_.planetCenter;
                float upLen = glm::length(up);
                if (upLen > 1e-5f)
                    up /= upLen;
                else
                    up = Vec3(0, 1, 0);
                Vec3 surfacePoint = frameData_.planetCenter + up * frameData_.planetRadius;

                Vec4 clip = frameData_.viewProj * Vec4(surfacePoint, 1.0f);
                printf("[OceanDebug] surfacePoint=(%.3f,%.3f,%.3f) distToCam=%.3f\n",
                       surfacePoint.x, surfacePoint.y, surfacePoint.z,
                       glm::length(frameData_.cameraPos - surfacePoint));
                printf("[OceanDebug] clip=(%.6f,%.6f,%.6f,%.6f) ndc=(%.6f,%.6f,%.6f)\n",
                       clip.x, clip.y, clip.z, clip.w,
                       clip.w != 0.0f ? clip.x / clip.w : 0.0f,
                       clip.w != 0.0f ? clip.y / clip.w : 0.0f,
                       clip.w != 0.0f ? clip.z / clip.w : 0.0f);

                Vec4 camClip = frameData_.viewProj * Vec4(frameData_.cameraPos, 1.0f);
                printf("[OceanDebug] camClip=(%.6f,%.6f,%.6f,%.6f)\n",
                       camClip.x, camClip.y, camClip.z, camClip.w);
            }
        }
#endif
    }

    void OceanTessellationPipelinePass::updateUBO()
    {
        OceanFFTSpectrumUBO spectrumUBO{};
        spectrumUBO.frameTime = accumulatedTime_ * 0.001f; // ms -> s
        spectrumUBO.deltaTime = static_cast<float>(Engine::Get()->GetDelta().AsSeconds());
        spectrumUBO.A = fftSettings_.damping;
        spectrumUBO.gravity = fftSettings_.gravity;
        spectrumUBO.repeatTime = fftSettings_.repeatTime;
        spectrumUBO.damping = fftSettings_.damping;
        spectrumUBO.depth = fftSettings_.depth;
        spectrumUBO.lowCutoff = fftSettings_.lowCutoff;
        spectrumUBO.highCutoff = fftSettings_.highCutoff;
        spectrumUBO.seed = fftSettings_.seed;
        spectrumUBO.wind = fftSettings_.wind;
        spectrumUBO.lambda = fftSettings_.lambda;
        spectrumUBO.normalStrength = fftSettings_.normalStrength;
        spectrumUBO.N = fftSettings_.N;
        spectrumUBO.lengthScale0 = static_cast<uint32_t>(fftSettings_.lengthScale0);
        spectrumUBO.lengthScale1 = static_cast<uint32_t>(fftSettings_.lengthScale1);
        spectrumUBO.lengthScale2 = static_cast<uint32_t>(fftSettings_.lengthScale2);
        spectrumUBO.lengthScale3 = static_cast<uint32_t>(fftSettings_.lengthScale3);
        spectrumUBO.foamBias = fftSettings_.foamBias;
        spectrumUBO.foamDecayRate = fftSettings_.foamDecayRate;
        spectrumUBO.foamAdd = fftSettings_.foamAdd;
        spectrumUBO.foamThreshold = fftSettings_.foamThreshold;

        spectrumUBO_->Update(spectrumUBO);
        ubo_->Update(frameData_);
    }

    void OceanTessellationPipelinePass::Render(const CommandBuffer &commandBuffer)
    {
        UpdateFrameData();
        updateUBO();

        RunFFTPass(commandBuffer);

        pipeline_->BindPipeline(commandBuffer);
        descSet_->BindDescriptor(commandBuffer);
        clipmapMesh_->Draw(commandBuffer);
    }

} // namespace SF::Engine