#include "Particle.si"  // Contains Particle, EmitterParams definitions

struct PushConstants
{
    float deltaTime;
    float time;
    uint  emitterCount;
    uint  _pad;
};

[[vk::binding(0, 0)]]
RWStructuredBuffer<Particle> particles;

[[vk::binding(1, 0)]]
StructuredBuffer<EmitterParams> emitters;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> freelist;

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

uint wangHash(uint seed)
{
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

// Returns a float in [0, 1)
float randFloat(inout uint seed)
{
    seed = wangHash(seed);
    return float(seed) * (1.0 / 4294967296.0);
}

// Returns a float in [lo, hi)
float randRange(inout uint seed, float lo, float hi)
{
    return lo + randFloat(seed) * (hi - lo);
}

// Returns a random direction inside a cone defined by (axis, halfAngle).
float3 randConeDir(inout uint seed, float3 axis, float halfAngle)
{
    // Sample a random azimuth and elevation within the cone.
    float theta = randFloat(seed) * 6.28318530718;   // 2pi
    float phi   = randFloat(seed) * halfAngle;

    // Build an arbitrary perpendicular basis around `axis`.
    float3 helper = abs(axis.x) < 0.9 ? float3(1, 0, 0) : float3(0, 1, 0);
    float3 perp0  = normalize(cross(axis, helper));
    float3 perp1  = cross(axis, perp0);

    float sinPhi = sin(phi);
    return normalize(axis * cos(phi) + perp0 * (sinPhi * cos(theta))
                                     + perp1 * (sinPhi * sin(theta)));
}

void spawnParticle(uint idx, EmitterParams e, uint seed)
{
    float lifetime = randRange(seed, e.minLifetime, e.maxLifetime);
    float speed    = randRange(seed, e.minSpeed,    e.maxSpeed);
    float size     = randRange(seed, e.minSize,     e.maxSize);

    float3 dir = randConeDir(seed, e.direction.xyz, e.direction.w);
    float3 vel = dir * speed;

    particles[idx].position  = float4(e.position.xyz, lifetime);
    particles[idx].velocity  = float4(vel, lifetime);        // w stores total lifetime
    particles[idx].color     = e.colorStart;
    particles[idx].size      = float2(size, size);
    particles[idx].rotation  = randFloat(seed) * 6.28318530718;
}

[numthreads(256, 1, 1)]
void main(uint3 globalThreadID : SV_DispatchThreadID)
{
    uint idx = globalThreadID.x;

    // Guard: threads beyond the particle budget do nothing.
    // MAX_TOTAL_PARTICLES is baked into the dispatch count on the CPU.
    // We rely on the CPU ensuring groups*256 >= MAX_TOTAL_PARTICLES and
    // simply discard out-of-range threads.
    uint particleCount;
    uint stride;
    particles.GetDimensions(particleCount, stride);
    if (idx >= particleCount)
        return;

    Particle p = particles[idx];

    float lifetimeRemaining = p.position.w;
    // Branch A: particle is alive.
    if (lifetimeRemaining > 0.0)
    {
        float totalLifetime = p.velocity.w;
        float t = 1.0 - (lifetimeRemaining / totalLifetime); // 0 = just spawned, 1 = dying

        // Gravity (emitter-defined; read from emitter that owns this slot).
        // We embed the owning emitter index in the particle's velocity.w slot
        // via a convention: velocity.w > 0 means it's a lifetime store, so we
        // need gravity from the emitter buffer. We iterate active emitters to
        // find the one that covers this particle index.
        // This is O(emitterCount) but emitterCount << 256.
        float gravity = -9.8; // default
        for (uint e = 0; e < pc.emitterCount; ++e)
        {
            uint base = emitters[e].emitterIndex * emitters[e].maxParticles;
            if (idx >= base && idx < base + emitters[e].maxParticles)
            {
                gravity = emitters[e].gravity;

                // Colour interpolation
                p.color = lerp(emitters[e].colorStart, emitters[e].colorEnd, t);

                // Size shrink: linear fade toward zero
                p.size.x = p.size.y * (1.0 - t);
                break;
            }
        }

        // Verlet integration (simple; replace with RK4 if needed)
        p.velocity.xyz += float3(0.0, gravity, 0.0) * pc.deltaTime;
        p.position.xyz += p.velocity.xyz * pc.deltaTime;

        // Slow rotation over time
        p.rotation += 0.5 * pc.deltaTime;

        // Decrement lifetime
        p.position.w -= pc.deltaTime;

        particles[idx] = p;
        return;
    }

    // Branch B: particle is dead, try to claim it for an emitter that needs to spawn a new one.
    // We use a lock-free approach: atomically increment a shared counter to claim a "spawn ticket".
    // Each emitter's allowed spawn count this frame is floor(emissionRate * deltaTime + accumulator), and the compute shader distributes tickets across the particle pool via fmodulo.
    // No CPU readback is required.
    for (uint e = 0; e < pc.emitterCount; ++e)
    {
        EmitterParams em = emitters[e];
        if (em.emissionRate <= 0.0)
            continue;

        // Slots owned by this emitter
        uint base      = em.emitterIndex * em.maxParticles;
        uint slotCount = em.maxParticles;

        if (idx < base || idx >= base + slotCount)
            continue;

        // How many new particles should this emitter spawn this frame?
        float desiredF = em.emissionRate * pc.deltaTime;
        uint  desired  = uint(desiredF + 0.5); // round to nearest
        if (desired == 0u)
            break;

        // Claim one spawn ticket atomically.
        uint ticket;
        InterlockedAdd(freelist[0], 1u, ticket);
        if (ticket >= desired)
        {
            // All tickets for this emitter already claimed this frame.
            break;
        }

        // Unique per-thread seed derived from particle index + frame time.
        uint seed = wangHash(idx ^ uint(pc.time * 1000.0));

        spawnParticle(idx, em, seed);
        break;
    }
}