#include "ParticleSystem.hpp"

#include <Engine/Log/Log.hpp>
#include <Rendering/RenderSystem.hpp>

#include <stdexcept>
#include <numeric>

namespace SF::Engine
{
    ParticleSystem::ParticleSystem()
    {
        // Populate the free-handle pool in reverse so handle 0 is handed out first.
        freeHandles_.resize(MAX_EMITTERS);
        std::iota(freeHandles_.rbegin(), freeHandles_.rend(), 0u);

        // Initialise all emitter slots as inactive.
        for (auto &e : emitters_)
            e.active = false;

        CreateBuffers();
        CreatePipeline();
        CreateDescriptorSet();
        WriteDescriptors();

        Log::Info("[ParticleSystem] Initialised. Budget: {} particles, {} emitters.",
                  MAX_TOTAL_PARTICLES, MAX_EMITTERS);
    }

    void ParticleSystem::Update()
    {
        if (activeEmitterCount_ == 0)
            return;

        static auto lastTime = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        elapsedTime_ += dt;

        for (auto &e : emitters_)
        {
            if (!e.active)
                continue;
            e.accumulator += e.params.emissionRate * dt;
        }

        UploadEmitterParams();
        DispatchSimulation(dt);
    }

    ParticleSystem::EmitterHandle ParticleSystem::AddEmitter(const EmitterParams &params)
    {
        if (freeHandles_.empty())
        {
            Log::Warning("[ParticleSystem] Emitter capacity ({}) reached.", MAX_EMITTERS);
            return INVALID_EMITTER;
        }

        const EmitterHandle h = freeHandles_.back();
        freeHandles_.pop_back();

        emitters_[h].params = params;
        emitters_[h].params.emitterIndex = h;
        // Assign each emitter an equal slice of the global particle pool.
        emitters_[h].params.maxParticles = MAX_TOTAL_PARTICLES / MAX_EMITTERS;
        emitters_[h].active = true;
        emitters_[h].accumulator = 0.0f;

        ++activeEmitterCount_;
        return h;
    }

    void ParticleSystem::RemoveEmitter(EmitterHandle handle)
    {
        if (handle >= MAX_EMITTERS || !emitters_[handle].active)
            return;

        emitters_[handle].active = false;
        freeHandles_.push_back(handle);
        --activeEmitterCount_;
    }

    ParticleEmitter &ParticleSystem::GetEmitter(EmitterHandle handle)
    {
        if (handle >= MAX_EMITTERS)
            throw std::out_of_range("[ParticleSystem] Invalid emitter handle.");
        return emitters_[handle];
    }

    void ParticleSystem::CreateBuffers()
    {
        particleBuffer_ = std::make_unique<Buffer>(
            sizeof(GpuParticle) * MAX_TOTAL_PARTICLES,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

        emitterBuffer_ = std::make_unique<Buffer>(
            sizeof(EmitterParams) * MAX_EMITTERS,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                VMA_ALLOCATION_CREATE_MAPPED_BIT);

        const uint32_t initialHead = MAX_TOTAL_PARTICLES;
        freelistBuffer_ = std::make_unique<Buffer>(
            sizeof(uint32_t),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_AUTO,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            std::as_bytes(std::span{&initialHead, 1}));

        Log::Info("[ParticleSystem] Buffers created. Particle SSBO: {} MiB",
                  (sizeof(GpuParticle) * MAX_TOTAL_PARTICLES) / (1024 * 1024));
    }

    void ParticleSystem::CreatePipeline()
    {
        computePipeline_ = std::make_unique<ComputePipeline>(
            "Shaders/Particles/Particle.shader");
    }

    void ParticleSystem::CreateDescriptorSet()
    {
        descriptorSet_ = std::make_unique<DescriptorSet>(*computePipeline_);
    }

    void ParticleSystem::WriteDescriptors()
    {
        VkDescriptorBufferInfo particleInfo{};
        particleInfo.buffer = particleBuffer_->GetBuffer();
        particleInfo.offset = 0;
        particleInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet writeParticle{};
        writeParticle.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeParticle.dstSet = descriptorSet_->GetDescriptorSet();
        writeParticle.dstBinding = 0;
        writeParticle.descriptorCount = 1;
        writeParticle.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeParticle.pBufferInfo = &particleInfo;

        VkDescriptorBufferInfo emitterInfo{};
        emitterInfo.buffer = emitterBuffer_->GetBuffer();
        emitterInfo.offset = 0;
        emitterInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet writeEmitter{};
        writeEmitter.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeEmitter.dstSet = descriptorSet_->GetDescriptorSet();
        writeEmitter.dstBinding = 1;
        writeEmitter.descriptorCount = 1;
        writeEmitter.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeEmitter.pBufferInfo = &emitterInfo;

        VkDescriptorBufferInfo freelistInfo{};
        freelistInfo.buffer = freelistBuffer_->GetBuffer();
        freelistInfo.offset = 0;
        freelistInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet writeFreelist{};
        writeFreelist.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeFreelist.dstSet = descriptorSet_->GetDescriptorSet();
        writeFreelist.dstBinding = 2;
        writeFreelist.descriptorCount = 1;
        writeFreelist.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeFreelist.pBufferInfo = &freelistInfo;

        DescriptorSet::Update({writeParticle, writeEmitter, writeFreelist});
    }

    void ParticleSystem::UploadEmitterParams()
    {
        // The emitter buffer is persistently mapped (VMA_ALLOCATION_CREATE_MAPPED_BIT).
        VmaAllocationInfo info = emitterBuffer_->GetAllocationInfo();
        auto *dst = static_cast<EmitterParams *>(info.pMappedData);

        for (uint32_t i = 0; i < MAX_EMITTERS; ++i)
        {
            if (emitters_[i].active)
                dst[i] = emitters_[i].params;
            else
                dst[i].emissionRate = 0.0f; // shader skips emitters with zero rate
        }
        // Buffer is VK_MEMORY_PROPERTY_HOST_COHERENT_BIT so no explicit flush required.
    }

    void ParticleSystem::DispatchSimulation(float dt)
    {
        CommandBuffer cmd(/*begin=*/true, VK_QUEUE_GRAPHICS_BIT,
                          VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        Buffer::InsertMemoryBarrier(
            cmd,
            particleBuffer_->GetBuffer(),
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        vkCmdBindPipeline(cmd,
                          VK_PIPELINE_BIND_POINT_COMPUTE,
                          computePipeline_->GetPipeline());

        descriptorSet_->BindDescriptor(cmd);

        ParticlePushConstants pc{};
        pc.deltaTime = dt;
        pc.time = elapsedTime_;
        pc.emitterCount = activeEmitterCount_;

        vkCmdPushConstants(cmd,
                           computePipeline_->GetPipelineLayout(),
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           sizeof(ParticlePushConstants),
                           &pc);

        constexpr uint32_t LOCAL_SIZE = 256;
        const uint32_t groups = (MAX_TOTAL_PARTICLES + LOCAL_SIZE - 1) / LOCAL_SIZE;
        vkCmdDispatch(cmd, groups, 1, 1);

        Buffer::InsertMemoryBarrier(
            cmd,
            particleBuffer_->GetBuffer(),
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

        cmd.SubmitIdle();
    }

} // namespace SF::Engine