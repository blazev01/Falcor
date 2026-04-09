add_falcor_executable(AnitoPlume)

target_sources(AnitoPlume PRIVATE
    AnitoPlume.cpp
    AnitoPlume.h
    AnitoPlume.3d.slang
    AnitoPlume.rt.slang
    Particles.cs.slang
    ParticleSystem.cpp
    ParticleSystem.h
)

target_copy_shaders(AnitoPlume Samples/AnitoPlume)

target_source_group(AnitoPlume "Samples")
