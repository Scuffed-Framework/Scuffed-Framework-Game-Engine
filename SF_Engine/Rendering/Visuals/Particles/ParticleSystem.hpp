#pragma once

#include <Engine/Module.hpp>
#include <Scene/Scene.hpp>

#include <Rendering/Buffers/Buffer.hpp>
#include <Rendering/Commands/CommandBuffer.hpp>
#include <Rendering/Descriptors/DescriptorSet.hpp>
#include <Rendering/Pipelines/ComputePipeline.hpp>

#include <Math/Vectors/Vector.hpp>
#include <Math/BasicMath.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace SF::Engine
{

    // GPU-side structures, must match particle.comp layout exactly.
    // All fields are 4-byte aligned; no implicit padding surprises.

    /**
     * @brief Per-particle state residing entirely on the GPU.
     *
     * Layout mirrors the GLSL struct in particle.comp:
     *   layout(set=0, binding=0) buffer ParticleBuffer { Particle particles[]; };
     */
    struct alignas(16) GpuParticle
    {
        Vec4 position; // xyz = world pos,  w = lifetime remaining (s)
        Vec4 velocity; // xyz = velocity,   w = total lifetime (s)
        Vec4 color;    // rgba, linear
        Vec2 size;     // x = current size, y = initial size
        float rotation;     // radians
        float _pad0;
    };

    /**
     * @brief Per-emitter parameters pushed to the GPU once per frame.
     *
     * Kept deliberately small: the compute shader reads one EmitterParams
     * entry per work-group, so false sharing is not a concern.
     */
    struct alignas(16) EmitterParams
    {
        Vec4 position;   // xyz = world origin, w = unused
        Vec4 direction;  // xyz = cone axis (normalised), w = half-angle (rad)
        Vec4 colorStart; // rgba
        Vec4 colorEnd;   // rgba
        float emissionRate;   // particles / second
        float minLifetime;    // seconds
        float maxLifetime;    // seconds
        float minSpeed;
        float maxSpeed;
        float minSize;
        float maxSize;
        float gravity;         // world-space Y acceleration (negative = down)
        uint32_t maxParticles; // hard cap for this emitter's slice
        uint32_t emitterIndex; // index into the global particle pool
        float _pad0;
        float _pad1;
    };

    /**
     * @brief Push-constant block sent every dispatch.
     * Binding: layout(push_constant) uniform PushConstants { ... };
     */
    struct ParticlePushConstants
    {
        float deltaTime;
        float time; // total elapsed time (for noise / patterns)
        uint32_t emitterCount;
        uint32_t _pad;
    };

    // Emitter, CPU-side handle owned by game code.

    /**
     * @brief Describes one particle emitter.
     *
     * The ParticleSystem keeps a flat array of these and uploads them to the
     * GPU each frame via a persistently-mapped staging buffer.
     */
    struct ParticleEmitter
    {
        EmitterParams params;

        bool active = true;
        float accumulator = 0.0f; // fractional particle debt (sub-frame emission)
    };

    // ParticleSystem module

    /**
     * @brief Data-oriented, GPU-driven particle system.
     *
     * Design goals:
     *   - Zero CPU particle iteration. Simulation runs in a single compute dispatch.
     *   - One persistent GPU buffer for all particle state (position, velocity,
     *     colour, lifetime). No per-frame uploads of individual particles.
     *   - A small persistently-mapped emitter params buffer for CPU→GPU emitter
     *     configuration (one EmitterParams per active emitter, re-uploaded each frame).
     *   - An atomic freelist counter buffer on the GPU for lock-free dead-particle
     *     recycling inside the compute shader (no CPU readback required).
     *   - Rendering is left to an injected pipeline pass (ParticlePipelinePass)
     *     that reads the same particle buffer; the system itself has no render
     *     pipeline dependency.
     *
     * GPU buffer layout:
     *   binding 0 , ParticleBuffer   : GpuParticle[MAX_TOTAL_PARTICLES]   (SSBO)
     *   binding 1 , EmitterBuffer    : EmitterParams[MAX_EMITTERS]         (SSBO)
     *   binding 2 , FreelistBuffer   : uint32_t  (atomic counter head)     (SSBO)
     *
     * Usage from game code:
     * @code
     *   auto *ps = ParticleSystem::Get();
     *   EmitterHandle h = ps->AddEmitter(params);
     *   ps->GetEmitter(h).params.position = Vec4(pos, 1.0f);
     *   ps->RemoveEmitter(h);
     * @endcode
     */
    class ParticleSystem final : public ModuleRegistrar<ParticleSystem>
    {
        friend class ModuleRegistrar<ParticleSystem>;
        REGISTER_MODULE(ParticleSystem, Module::Stage::Render, Module::Requires<>{});

    public:
        // consts
        static constexpr uint32_t MAX_TOTAL_PARTICLES = 5'000'000; // should be enough
        static constexpr uint32_t MAX_EMITTERS = 256;

        using EmitterHandle = uint32_t;
        static constexpr EmitterHandle INVALID_EMITTER = ~0u;

        ParticleSystem();
        ~ParticleSystem() override = default;

        void Update() override;

        /**
         * @brief Register a new emitter. Returns INVALID_EMITTER if the
         *        system is at capacity (MAX_EMITTERS).
         */
        EmitterHandle AddEmitter(const EmitterParams &params);

        /**
         * @brief Deactivate and recycle an emitter slot.
         */
        void RemoveEmitter(EmitterHandle handle);

        /**
         * @brief Direct mutable access to an emitter (e.g. to move it).
         *        The dirty flag is set automatically on the next Update().
         */
        ParticleEmitter &GetEmitter(EmitterHandle handle);

        /**
         * @brief Read-only access to the GPU particle buffer for render passes.
         */
        const Buffer &GetParticleBuffer() const { return *particleBuffer_; }

        /**
         * @brief Current number of active emitters.
         */
        uint32_t GetActiveEmitterCount() const { return activeEmitterCount_; }

    private:
        void CreateBuffers();
        void CreatePipeline();
        void CreateDescriptorSet();
        void WriteDescriptors();

        void UploadEmitterParams();
        void DispatchSimulation(float dt);

        std::unique_ptr<Buffer> particleBuffer_; // GpuParticle[MAX_TOTAL_PARTICLES]
        std::unique_ptr<Buffer> emitterBuffer_;  // EmitterParams[MAX_EMITTERS]  (host-visible)
        std::unique_ptr<Buffer> freelistBuffer_; // uint32_t  atomic counter

        std::unique_ptr<ComputePipeline> computePipeline_;
        std::unique_ptr<DescriptorSet> descriptorSet_;

        std::array<ParticleEmitter, MAX_EMITTERS> emitters_{};
        std::vector<EmitterHandle> freeHandles_;
        uint32_t activeEmitterCount_ = 0;

        float elapsedTime_ = 0.0f;
    };

} // namespace SF::Engine