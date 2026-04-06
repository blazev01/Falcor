/***************************************************************************
 # Copyright (c) 2015-24, NVIDIA CORPORATION. All rights reserved.
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
/**
 * ParticleSimulate.cs.slang
 *
 * Integrates every particle in the pool, compacts live ones into AliveList[],
 * and writes the final indirect draw vertex count into gDrawArgs.
 *
 * One thread = one pool slot.
 */

struct GpuParticle
{
    float3 posW;
    float size;
    float3 velocity;
    float life;
    float maxLife;
    float3 pad;
    float4 color;
};

RWStructuredBuffer<GpuParticle> gParticlePool;
RWStructuredBuffer<uint> gAliveList; ///< Output: compact live indices
RWStructuredBuffer<uint> gLiveCount; ///< Atomic live counter (cleared to 0 by CPU)
RWByteAddressBuffer gDrawArgs;       ///< DrawIndirectArgs  (16 bytes)

cbuffer SimCB
{
    uint gMaxParticles;
    float gDeltaTime;
    float3 gGravity;
};

[numthreads(64, 1, 1)]
void csSimulate(uint3 dispatchID: SV_DispatchThreadID)
{
    uint idx = dispatchID.x;
    if (idx >= gMaxParticles) return;

    GpuParticle p = gParticlePool[idx];

    // Skip dead particles
    if (p.life <= 0.f) return;

    // ── integrate ──────────────────────────────────────────────────────
    p.velocity += gGravity * gDeltaTime;
    p.posW += p.velocity * gDeltaTime;
    p.life -= gDeltaTime;

    if (p.life <= 0.f)
    {
        p.life = 0.f;
        gParticlePool[idx] = p;
        return; // particle just died – don't add to alive list
    }

    // Quadratic alpha fade
    float t = saturate(p.life / p.maxLife);
    p.color.a = t * t;

    gParticlePool[idx] = p;

    // ── append to alive list ────────────────────────────────────────────
    uint aliveSlot;
    InterlockedAdd(gLiveCount[0], 1u, aliveSlot);
    gAliveList[aliveSlot] = idx;

    // ── write vertex count into DrawIndirectArgs[0] ─────────────────────
    // Layout at byte 0: VertexCountPerInstance  (we write live count + 1 here
    // but that races; the pattern is to write it once after all threads finish.
    // Instead we rely on the fact that gLiveCount[0] IS the vertex count and
    // the CPU passes it via a copy just before the draw — OR we use the
    // InterlockedMax trick below so the last thread wins the correct value.)
    //
    // Simplest correct approach: after the dispatch the C++ side copies
    // gLiveCount → gDrawArgs[0].  See the C++ execute() for the copy.
    // We still write InstanceCount=1 here (only needs to be set once).
    gDrawArgs.Store(4, 1u);  // InstanceCount  = 1
    gDrawArgs.Store(8, 0u);  // StartVertex    = 0
    gDrawArgs.Store(12, 0u); // StartInstance  = 0
}
