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
#pragma once
#include "Falcor.h"
#include "RenderGraph/RenderPass.h"
#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class ParticleCompute : public RenderPass
{
public:
    FALCOR_PLUGIN_CLASS(ParticleCompute, "ParticleCompute", "Insert pass description here.");

    static ref<ParticleCompute> create(ref<Device> pDevice, const Properties& props)
    {
        return make_ref<ParticleCompute>(pDevice, props);
    }

    ParticleCompute(ref<Device> pDevice, const Properties& props);

    virtual Properties getProperties() const override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pRenderContext, const CompileData& compileData) override {}
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual void setScene(RenderContext* pRenderContext, const ref<Scene>& pScene) override {}
    virtual bool onMouseEvent(const MouseEvent& mouseEvent) override { return false; }
    virtual bool onKeyEvent(const KeyboardEvent& keyEvent) override { return false; }

private:
    // ── tuneable params ────────────────────────────────────────────────
    uint32_t mMaxParticles = 1u << 16; ///< Pool size (must be pow-2 friendly)
    float mSpawnRate = 300.f;          ///< Particles emitted per second
    float mSpawnAccum = 0.f;           ///< Sub-frame accumulator
    float3 mEmitterPos = {0, 0, 0};
    float mInitialSpeed = 2.5f;
    float mParticleSize = 0.07f;
    float mMaxLifetime = 3.f;
    float3 mGravity = {0.f, -3.f, 0.f};
    uint32_t mFrameIndex = 0u; ///< Used as RNG seed variation

    // ── GPU buffers ────────────────────────────────────────────────────

    /// Full particle state.  Layout == struct GpuParticle in the shader.
    ref<Buffer> mpParticlePool; // RWStructuredBuffer<GpuParticle>[mMaxParticles]

    /// Compact index list of live particles built by Simulate each frame.
    ref<Buffer> mpAliveList; // RWStructuredBuffer<uint>[mMaxParticles]

    /// Single uint: next free slot for Emit (persistent across frames).
    ref<Buffer> mpNextSlot; // RWStructuredBuffer<uint>[1]

    /// Single uint: live count written by Simulate, read as draw vertex count.
    ref<Buffer> mpLiveCount; // RWStructuredBuffer<uint>[1]

    /// DrawIndexedIndirectArguments layout used by Falcor indirect draw:
    ///   { VertexCountPerInstance, InstanceCount, StartVertex, StartInstance }
    ref<Buffer> mpDrawArgs; // RWBuffer<uint4> (16 bytes)

    // ── compute passes ────────────────────────────────────────────────
    ref<ComputePass> mpEmitPass;
    ref<ComputePass> mpSimulatePass;

    // ── raster resources ──────────────────────────────────────────────
    ref<GraphicsState> mpGfxState;
    ref<ProgramVars> mpGfxVars;
    ref<BlendState> mpBlendState;
    ref<DepthStencilState> mpDepthState;
    ref<RasterizerState> mpRasterState;
    ref<Texture> mpParticleTex; ///< Optional sprite texture

    ref<Scene> mpScene;
    double mLastTime = -1.0;

    void prepareResources();
};
