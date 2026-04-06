/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "ParticleCompute.h"
#include "RenderGraph/RenderPassStandardFlags.h"

namespace
{
// ── render graph I/O ─────────────────────────────────────────────────
const std::string kColorOut = "colorOut";
const std::string kDepthIn = "depth";

// ── shader files ──────────────────────────────────────────────────────
const std::string kEmitShader = "RenderPasses/BillboardParticlePass/ParticleEmit.cs.slang";
const std::string kSimulateShader = "RenderPasses/BillboardParticlePass/ParticleSimulate.cs.slang";
const std::string kRenderShader = "RenderPasses/BillboardParticlePass/ParticleRender.3d.slang";

// ── thread-group size (must match [numthreads] in the shader) ─────────
constexpr uint32_t kSimGroupSize = 64u;
} // namespace

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, ParticleCompute>();
}

ParticleCompute::ParticleCompute(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice) {}

Properties ParticleCompute::getProperties() const
{
    return {};
}

RenderPassReflection ParticleCompute::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInputOutput(kColorOut, "Colour buffer; particles alpha-blended on top")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(ResourceBindFlags::RenderTarget);
    reflector.addInput(kDepthIn, "Scene depth for occlusion")
        .format(ResourceFormat::D32Float)
        .bindFlags(ResourceBindFlags::DepthStencil)
        .flags(RenderPassReflection::Field::Flags::Optional);
    return reflector;
}

void ParticleCompute::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // ── delta time ──────────────────────────────────────────────────────
    using Clock = std::chrono::steady_clock;
    double now = (double)std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch()).count() * 1e-6;
    float dt = (mLastTime < 0.0) ? (1.f / 60.f) : std::min((float)(now - mLastTime), 0.1f);
    mLastTime = now;
    ++mFrameIndex;

    // ── how many particles to spawn this frame ──────────────────────────
    mSpawnAccum += mSpawnRate * dt;
    uint32_t toSpawn = (uint32_t)mSpawnAccum;
    mSpawnAccum -= (float)toSpawn;
    toSpawn = std::min(toSpawn, mMaxParticles);

    // ──────────────────────────────────────────────────────────────────────
    //  1. EMIT PASS
    // ──────────────────────────────────────────────────────────────────────
    {

        //auto var = mpEmitPass->getVars();
        //var["EmitCB"]["gToSpawn"] = toSpawn;
        //var["EmitCB"]["gMaxParticles"] = mMaxParticles;
        //var["EmitCB"]["gEmitterPos"] = mEmitterPos;
        //var["EmitCB"]["gInitSpeed"] = mInitialSpeed;
        //var["EmitCB"]["gSize"] = mParticleSize;
        //var["EmitCB"]["gMaxLife"] = mMaxLifetime;
        //var["EmitCB"]["gFrameSeed"] = mFrameIndex;
        //var["gParticlePool"] = mpParticlePool;
        //var["gNextSlot"] = mpNextSlot;

        // One thread per particle to spawn; group size 64
        uint32_t groups = std::max(1u, (toSpawn + 63u) / 64u);
        mpEmitPass->execute(pRenderContext, groups, 1, 1);
    }

    // UAV barrier between Emit and Simulate
    pRenderContext->uavBarrier(mpParticlePool.get());
    pRenderContext->uavBarrier(mpNextSlot.get());

    // ──────────────────────────────────────────────────────────────────────
    //  2. SIMULATE PASS
    //     Reset the live counter to 0 first (CPU-side clear on the UAV).
    // ──────────────────────────────────────────────────────────────────────
    {
        // Clear live count and draw args to zero
        const uint32_t zero[4] = {0, 0, 0, 0};
        pRenderContext->clearUAV(mpLiveCount->getUAV().get(), uint4(0, 0, 0, 0));

        //auto var = mpSimulatePass->getVars();
        //var["SimCB"]["gMaxParticles"] = mMaxParticles;
        //var["SimCB"]["gDeltaTime"] = dt;
        //var["SimCB"]["gGravity"] = mGravity;
        //var["gParticlePool"] = mpParticlePool;
        //var["gAliveList"] = mpAliveList;
        //var["gLiveCount"] = mpLiveCount;
        //var["gDrawArgs"] = mpDrawArgs;

        uint32_t groups = (mMaxParticles + kSimGroupSize - 1) / kSimGroupSize;
        mpSimulatePass->execute(pRenderContext, groups, 1, 1);
    }

    pRenderContext->uavBarrier(mpParticlePool.get());
    pRenderContext->uavBarrier(mpAliveList.get());
    pRenderContext->uavBarrier(mpLiveCount.get());
    pRenderContext->uavBarrier(mpDrawArgs.get());

    // ──────────────────────────────────────────────────────────────────────
    //  3. BILLBOARD RENDER  (indirect draw)
    // ──────────────────────────────────────────────────────────────────────
    auto pColorTex = renderData.getTexture(kColorOut);
    auto pDepthTex = renderData.getTexture(kDepthIn);

    ref<Fbo> pFbo = Fbo::create(mpDevice);
    pFbo->attachColorTarget(pColorTex, 0);
    if (pDepthTex)
        pFbo->attachDepthStencilTarget(pDepthTex);
    mpGfxState->setFbo(pFbo);

    // Camera matrices
    float4x4 view = float4x4::identity();
    float4x4 proj = float4x4::identity();
    if (mpScene && mpScene->getCamera())
    {
        view = mpScene->getCamera()->getViewMatrix();
        proj = mpScene->getCamera()->getProjMatrix();
    }
    float3 camRight = float3(view[0][0], view[1][0], view[2][0]);
    float3 camUp = float3(view[0][1], view[1][1], view[2][1]);

    //auto gvars = mpGfxVars;
    //gvars["RenderCB"]["gViewProj"] = mul(proj, view);
    //gvars["RenderCB"]["gCamRight"] = camRight;
    //gvars["RenderCB"]["gCamUp"] = camUp;
    //gvars["gParticlePool"] = mpParticlePool;
    //gvars["gAliveList"] = mpAliveList;
    //if (mpParticleTex)
    //    gvars["gParticleTex"] = mpParticleTex;

    // Indirect draw: vertex count was written by the Simulate shader
    pRenderContext->drawIndirect(
        mpGfxState.get(),
        mpGfxVars.get(),
        1,                // draw count
        mpDrawArgs.get(), // buffer containing DrawIndirectArgs
        0,                // byte offset
        nullptr,
        0
    );
}

void ParticleCompute::renderUI(Gui::Widgets& widget)
{
    widget.var("Spawn rate (p/s)", mSpawnRate, 1.f, 5000.f, 10.f);
    widget.var("Initial speed", mInitialSpeed, 0.1f, 20.f, 0.1f);
    widget.var("Particle size", mParticleSize, 0.01f, 2.f, 0.01f);
    widget.var("Lifetime (s)", mMaxLifetime, 0.1f, 10.f, 0.1f);
    widget.var("Gravity", mGravity, -20.f, 20.f, 0.1f);
    widget.var("Emitter pos", mEmitterPos, -50.f, 50.f, 0.1f);
}

void ParticleCompute::prepareResources()
{
    const uint32_t N = mMaxParticles;

    // ── particle pool ─────────────────────────────────────────────────────
    // Struct layout must match GpuParticle in the shaders:
    //   float3 posW, float size, float3 velocity, float life,
    //   float maxLife, float pad[3], float4 color   →  64 bytes
    mpParticlePool = mpDevice->createStructuredBuffer(
        64, // sizeof(GpuParticle)
        N,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess,
        MemoryType::DeviceLocal,
        nullptr,
        false
    );

    // ── alive list ────────────────────────────────────────────────────────
    mpAliveList = mpDevice->createStructuredBuffer(
        sizeof(uint32_t), N, ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal, nullptr, false
    );

    // ── atomic counters ───────────────────────────────────────────────────
    mpNextSlot =
        mpDevice->createStructuredBuffer(sizeof(uint32_t), 1, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal, nullptr, false);

    mpLiveCount =
        mpDevice->createStructuredBuffer(sizeof(uint32_t), 1, ResourceBindFlags::UnorderedAccess, MemoryType::DeviceLocal, nullptr, false);

    // ── indirect draw args buffer ─────────────────────────────────────────
    // Layout: { VertexCountPerInstance=0, InstanceCount=1,
    //           StartVertexLocation=0,   StartInstanceLocation=0 }
    const uint32_t initArgs[4] = {0, 1, 0, 0};
    mpDrawArgs = mpDevice->createBuffer(
        sizeof(uint32_t) * 4, ResourceBindFlags::UnorderedAccess | ResourceBindFlags::IndirectArg, MemoryType::DeviceLocal, initArgs
    );

    // ── compute passes ────────────────────────────────────────────────────
    mpEmitPass = ComputePass::create(mpDevice, kEmitShader, "csEmit");
    mpSimulatePass = ComputePass::create(mpDevice, kSimulateShader, "csSimulate");

    // ── raster program ────────────────────────────────────────────────────
    ProgramDesc pd;
    pd.addShaderLibrary(kRenderShader).vsEntry("vsMain").gsEntry("gsMain").psEntry("psMain");
    ref<Program> pProg = Program::create(mpDevice, pd, {});

    mpGfxState = GraphicsState::create(mpDevice);
    mpGfxState->setProgram(pProg);
    mpGfxState->setVao(Vao::create(Vao::Topology::PointList));

    // Alpha blending
    BlendState::Desc bd;
    bd.setRtBlend(0, true).setRtParams(
        0,
        BlendState::BlendOp::Add,
        BlendState::BlendOp::Add,
        BlendState::BlendFunc::SrcAlpha,
        BlendState::BlendFunc::OneMinusSrcAlpha,
        BlendState::BlendFunc::One,
        BlendState::BlendFunc::OneMinusSrcAlpha
    );
    mpBlendState = BlendState::create(bd);
    mpGfxState->setBlendState(mpBlendState);

    // Depth test ON, depth write OFF
    DepthStencilState::Desc dd;
    dd.setDepthEnabled(true).setDepthWriteMask(false);
    mpDepthState = DepthStencilState::create(dd);
    mpGfxState->setDepthStencilState(mpDepthState);

    // No culling
    RasterizerState::Desc rd;
    rd.setCullMode(RasterizerState::CullMode::None);
    mpRasterState = RasterizerState::create(rd);
    mpGfxState->setRasterizerState(mpRasterState);

    mpGfxVars = ProgramVars::create(mpDevice, pProg->getReflector());
}
