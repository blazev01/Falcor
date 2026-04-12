add_falcor_executable(AnitoPlume)

target_sources(AnitoPlume PRIVATE
    AnitoPlume.cpp
    AnitoPlume.h
    ParticleSystem.cpp
    ParticleSystem.h
    AnitoPlume.3d.slang
    AnitoPlume.rt.slang
    Particles.cs.slang
    ParticleBillboard.3d.slang
)

target_copy_shaders(AnitoPlume Samples/AnitoPlume)

target_source_group(AnitoPlume "Samples")
