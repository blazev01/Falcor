// ParticleSystem.h
// Drop-in particle system for a Falcor HelloDXR-based sample.
// Owns GPU buffers, compiles the compute passes, and drives each frame.

#pragma once
#include "Falcor.h"

using namespace Falcor;

// ─── Constants ────────────────────────────────────────────────────────────────

static constexpr uint32_t kMaxParticles  = 65536;
static constexpr uint32_t kEmitPerFrame  = 128;
static constexpr uint32_t kCounterBytes  = 8;  // 2 × uint32: deadCount, aliveCount

// ─── ParticleSystem ───────────────────────────────────────────────────────────

class ParticleSystem : public Object
{
public:
    FALCOR_OBJECT(ParticleSystem);

    static ref<ParticleSystem> create(){return make_ref<ParticleSystem>(); }

    ParticleSystem() = default;
    // Call once after your RenderContext is available.
    void init(RenderContext* pCtx, ref<Device> pDevice);
    
    // Call every frame inside your renderFrame() or execute() callback.
    void simulate(RenderContext* pCtx, float deltaTime);

    // Accessors for your renderer to build a billboard draw call
    ref<Buffer> getParticleBuffer() const { return mpParticleBuffer; }
    ref<Buffer> getAliveList()      const { return mpAliveList; }
    ref<Buffer> getCounters()       const { return mpCounters; }

    // ── Tuning knobs (set before each simulate() call if you like) ───────────
    float3 mEmitterPos     = { 0.f, 0.f, 0.f };
    float3 mEmitDirection  = { 0.f, 1.f, 0.f }; // upward
    float3 mGravity        = { 0.f, -9.8f, 0.f };
    float4 mStartColor     = { 1.f, 0.6f, 0.1f, 1.f }; // orange
    float4 mEndColor       = { 0.3f, 0.3f, 0.3f, 0.f }; // grey → transparent
    float  mEmitSpeed      = 4.f;
    float  mSpreadAngle    = 0.3f;  // radians (~17°)
    float  mMinLifetime    = 1.5f;
    float  mMaxLifetime    = 3.5f;
    float  mMinSize        = 0.05f;
    float  mMaxSize        = 0.15f;
    uint32_t mEmitPerFrame = kEmitPerFrame;

private:
    struct Particle  // must mirror the Slang struct exactly
    {
        float3   position;
        float    age;
        float3   velocity;
        float    lifetime;
        float4   color;
        float    size;
        uint32_t flags;
        float    _pad[2]; // keep 16-byte aligned → 64 bytes total
    };
    static_assert(sizeof(Particle) == 64, "Particle size mismatch with shader");

    ref<ComputePass>   mpEmitPass;
    ref<ComputePass>   mpUpdatePass;
    ref<Buffer>        mpParticleBuffer;
    ref<Buffer>        mpDeadList;
    ref<Buffer>        mpAliveList;
    ref<Buffer>        mpCounters;
    ref<ParameterBlock> mpVars;

    uint32_t mFrameSeed = 0u;

    static uint32_t div_round_up(uint32_t n, uint32_t d) { return (n + d - 1) / d; }
};
