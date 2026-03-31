add_plugin(PlumeCompute)

target_sources(PlumeCompute PRIVATE
    PlumeCompute.cpp
    PlumeCompute.h
    PlumeCompute.cs.slang
    SmokeLayer.slang
    Plume.slang
)

target_source_group(PlumeCompute "RenderPasses")
