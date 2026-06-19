#version 450
#extension GL_EXT_shader_atomic_float : enable

// Particle simulation compute shader
//
// One thread = one particle slot.
// Dispatch: ceil(MAX_TOTAL_PARTICLES / 256) work-groups of 256 threads each.
//
// Bindings:
//   0 : ParticleBuffer   : read/write particle state
//   1 : EmitterBuffer    : read-only emitter params (uploaded CPU→GPU each frame)
//   2 : FreelistBuffer   : atomic counter (dead-particle recycling, no CPU readback)
//
// Push constants:
//   deltaTime, time, emitterCount
// Local work-group size.
// 256 threads gives good occupancy on desktop GPUs while staying within the
// minimum maxComputeWorkGroupInvocations (128) guaranteed by Vulkan 1.0.
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct Particle
{
    vec4 position;  // xyz = world pos,  w = lifetime remaining
    vec4 velocity;  // xyz = velocity,   w = total lifetime
    vec4 color;     // rgba linear
    vec2 size;      // x = current, y = initial
    float rotation;
    float _pad0;
};

struct EmitterParams
{
    vec4  position;       // xyz = world origin
    vec4  direction;      // xyz = cone axis, w = half-angle (radians)
    vec4  colorStart;
    vec4  colorEnd;
    float emissionRate;
    float minLifetime;
    float maxLifetime;
    float minSpeed;
    float maxSpeed;
    float minSize;
    float maxSize;
    float gravity;
    uint  maxParticles;
    uint  emitterIndex;
    float _pad0;
    float _pad1;
};

layout(std430, set = 0, binding = 0) buffer ParticleBuffer
{
    Particle particles[];
};

layout(std430, set = 0, binding = 1) readonly buffer EmitterBuffer
{
    EmitterParams emitters[];
};

layout(std430, set = 0, binding = 2) buffer FreelistBuffer
{
    uint freelistHead;  // atomic counter; wraps around MAX_TOTAL_PARTICLES
};

layout(push_constant) uniform PushConstants
{
    float deltaTime;
    float time;
    uint  emitterCount;
    uint  _pad;
} pc;

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
vec3 randConeDir(inout uint seed, vec3 axis, float halfAngle)
{
    // Sample a random azimuth and elevation within the cone.
    float theta = randFloat(seed) * 6.28318530718;   // 2pi
    float phi   = randFloat(seed) * halfAngle;

    // Build an arbitrary perpendicular basis around `axis`.
    vec3 helper = abs(axis.x) < 0.9 ? vec3(1, 0, 0) : vec3(0, 1, 0);
    vec3 perp0  = normalize(cross(axis, helper));
    vec3 perp1  = cross(axis, perp0);

    float sinPhi = sin(phi);
    return normalize(axis * cos(phi) + perp0 * (sinPhi * cos(theta))
                                     + perp1 * (sinPhi * sin(theta)));
}

void spawnParticle(uint idx, EmitterParams e, uint seed)
{
    float lifetime = randRange(seed, e.minLifetime, e.maxLifetime);
    float speed    = randRange(seed, e.minSpeed,    e.maxSpeed);
    float size     = randRange(seed, e.minSize,     e.maxSize);

    vec3 dir = randConeDir(seed, e.direction.xyz, e.direction.w);
    vec3 vel = dir * speed;

    particles[idx].position  = vec4(e.position.xyz, lifetime);
    particles[idx].velocity  = vec4(vel, lifetime);        // w stores total lifetime
    particles[idx].color     = e.colorStart;
    particles[idx].size      = vec2(size, size);
    particles[idx].rotation  = randFloat(seed) * 6.28318530718;
}

void main()
{
    uint idx = gl_GlobalInvocationID.x;

    // Guard: threads beyond the particle budget do nothing.
    // MAX_TOTAL_PARTICLES is baked into the dispatch count on the CPU.
    // We rely on the CPU ensuring groups*256 >= MAX_TOTAL_PARTICLES and
    // simply discard out-of-range threads.
    if (idx >= uint(particles.length()))
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
                p.color = mix(emitters[e].colorStart, emitters[e].colorEnd, t);

                // Size shrink: linear fade toward zero
                p.size.x = p.size.y * (1.0 - t);
                break;
            }
        }

        // Verlet integration (simple; replace with RK4 if needed)
        p.velocity.xyz += vec3(0.0, gravity, 0.0) * pc.deltaTime;
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
    // Each emitter's allowed spawn count this frame is floor(emissionRate * deltaTime + accumulator), and the compute shader distributes tickets across the particle pool via modulo.
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
        uint ticket = atomicAdd(freelistHead, 1u);
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
