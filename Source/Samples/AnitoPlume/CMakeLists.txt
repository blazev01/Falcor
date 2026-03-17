add_falcor_executable(AnitoPlume)

target_sources(AnitoPlume PRIVATE
    AnitoPlume.cpp
    AnitoPlume.h
)

target_copy_shaders(AnitoPlume Samples/AnitoPlume)

target_source_group(AnitoPlume "Samples")
