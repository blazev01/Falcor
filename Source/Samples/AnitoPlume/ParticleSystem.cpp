#include "ParticleSystem.h"

using namespace Falcor;

void ParticleSystem::init(RenderContext* pCtx, ref<Device> pDevice)
{
    // ── Compile the two compute passes ──────────────────────────────────
    ProgramDesc emitDesc;
    emitDesc.addShaderLibrary("Samples/AnitoPlume/Particles.cs.slang").csEntry("emitParticles");
    mpEmitPass = ComputePass::create(pDevice, emitDesc);

    ProgramDesc updateDesc;
    updateDesc.addShaderLibrary("Samples/AnitoPlume/Particles.cs.slang").csEntry("updateParticles");
    mpUpdatePass = ComputePass::create(pDevice, updateDesc);

    // ── Particle buffer ─────────────────────────────────────────────────
    // Each Particle is (float3 pos, float age, float3 vel, float lifetime,
    //                   float4 color, float size, uint flags) = 15 floats + 1 uint = 64 bytes
    mpParticleBuffer = pDevice->createStructuredBuffer(
        sizeof(Particle), kMaxParticles,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType::DeviceLocal, nullptr, false);

    // ── Dead list: pre-fill with every index (all slots free at start) ─
    std::vector<uint32_t> deadIndices(kMaxParticles);
    std::iota(deadIndices.begin(), deadIndices.end(), 0);
    mpDeadList = pDevice->createStructuredBuffer(
        sizeof(uint32_t), kMaxParticles,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType::DeviceLocal, deadIndices.data(), false);

    // ── Alive list: output of update pass, read by renderer ─────────────
    mpAliveList = pDevice->createStructuredBuffer(
        sizeof(uint32_t), kMaxParticles,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType::DeviceLocal, nullptr, false);

    // ── Counter buffer: [0]=deadCount (init=kMaxParticles), [1]=aliveCount ─
    uint32_t initCounters[2] = { kMaxParticles, 0 };
    mpCounters = pDevice->createBuffer(
        kCounterBytes,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType::DeviceLocal, initCounters);

    mpVars = nullptr; // will be bound lazily below
}

// Call every frame inside your renderFrame() or execute() callback.
void ParticleSystem::simulate(RenderContext* pCtx, float deltaTime)
{
    mFrameSeed++;

    // ── Reset alive counter to 0 each frame before update pass ──────────
    // (dead counter is managed atomically by the shaders themselves)
    uint32_t zero = 0;
    pCtx->updateBuffer(mpCounters.get(), &zero, sizeof(uint32_t), sizeof(uint32_t));

    // ── Bind resources shared by both passes ─────────────────────────────
    auto bindCommon = [&](ShaderVar& vars) {
        vars["gParticles"] = mpParticleBuffer;
        vars["gDeadList"]  = mpDeadList;
        vars["gAliveList"] = mpAliveList;
        vars["gCounters"]  = mpCounters;

        auto cb = vars["PerFrameCB"];
        cb["gEmitterPos"]    = mEmitterPos;
        cb["gDeltaTime"]     = deltaTime;
        cb["gEmitDirection"] = mEmitDirection;
        cb["gEmitSpeed"]     = mEmitSpeed;
        cb["gGravity"]       = mGravity;
        cb["gSpreadAngle"]   = mSpreadAngle;
        cb["gStartColor"]    = mStartColor;
        cb["gEndColor"]      = mEndColor;
        cb["gMinLifetime"]   = mMinLifetime;
        cb["gMaxLifetime"]   = mMaxLifetime;
        cb["gMinSize"]       = mMinSize;
        cb["gMaxSize"]       = mMaxSize;
        cb["gEmitCount"]     = mEmitPerFrame;
        cb["gMaxParticles"]  = kMaxParticles;
        cb["gFrameSeed"]     = mFrameSeed;
    };

    // ── Emit pass: spawn new particles ──────────────────────────────────
    {
        ShaderVar vars = mpEmitPass->getRootVar();
        bindCommon(vars);
        uint32_t groups = div_round_up(mEmitPerFrame, 64u);
        mpEmitPass->execute(pCtx, groups, 1, 1);
    }

    // ── Update pass: simulate all slots ─────────────────────────────────
    {
        ShaderVar vars = mpUpdatePass->getRootVar();
        bindCommon(vars);
        uint32_t groups = div_round_up(kMaxParticles, 64u);
        mpUpdatePass->execute(pCtx, groups, 1, 1);
    }
}
