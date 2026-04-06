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
#include "PlumeCompute.h"

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, PlumeCompute>();
}

PlumeCompute::PlumeCompute(ref<Device> pDevice, const Properties& props) : RenderPass(pDevice)
{
    // Load and compile the .slang compute shader
    // ProgramDesc describes the shader source and entry point
    ProgramDesc desc;

    // entry point name matches [numthreads] function
    desc.addShaderLibrary("RenderPasses/PlumeCompute/PlumeCompute.cs.slang").csEntry("main");

    // ComputePass wraps the program + var bindings together
    mpComputePass = ComputePass::create(pDevice, desc);
}

Properties PlumeCompute::getProperties() const
{
    return {};
}

RenderPassReflection PlumeCompute::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    // Declare an input texture slot named "input"
    // The render graph wires this to the previous pass's output
    reflector.addInput("input", "Input color texture").bindFlags(ResourceBindFlags::ShaderResource).format(ResourceFormat::RGBA32Float);

    // Declare an output texture slot named "output"
    reflector.addOutput("output", "Processed output texture")
        .bindFlags(ResourceBindFlags::UnorderedAccess | ResourceBindFlags::ShaderResource)
        .format(ResourceFormat::RGBA32Float);

    return reflector;
}

void PlumeCompute::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // Grab textures from the render graph by their declared names
    ref<Texture> pInput = renderData.getTexture("input");
    ref<Texture> pOutput = renderData.getTexture("output");

    FALCOR_ASSERT(pInput && pOutput);

    // TODO: Add any additional resources (buffers, samplers, etc.) and bind them here

    // --- Bind shader variables ---
    // ShaderVar gives type-safe access to cbuffer/resource slots by name
    auto var = mpComputePass->getRootVar();

    // Set cbuffer fields (maps to PerFrameCB in the shader)
    //var["PerFrameCB"]["gResolution"] = uint2(pOutput->getWidth(), pOutput->getHeight());
    //var["PerFrameCB"]["gTime"] = mTime;

    // Bind textures (names match resource declarations in .slang)
    var["gInputTex"] = pInput;
    var["gOutputTex"] = pOutput;

    // Bind textures (names match resource declarations in .slang)
    var["gInputTex"] = pInput;
    var["gOutputTex"] = pOutput;

    // --- Dispatch ---
    // Calculate number of thread groups needed to cover the output
    uint32_t groupsX = div_round_up(pOutput->getWidth(), 0U);
    uint32_t groupsY = div_round_up(pOutput->getHeight(), 0U);

    mpComputePass->execute(pRenderContext, groupsX, groupsY, 1);
}

void PlumeCompute::renderUI(Gui::Widgets& widget)
{

}

void PlumeCompute::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    mpScene = pScene;
}
