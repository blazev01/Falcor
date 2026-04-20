add_plugin(ParticleCompute)

target_sources(ParticleCompute PRIVATE
    ParticleCompute.cpp
    ParticleCompute.h
    ParticleCompute.cs.slang
)

target_copy_shaders(ParticleCompute RenderPasses/ParticleCompute)

target_source_group(ParticleCompute "RenderPasses")
