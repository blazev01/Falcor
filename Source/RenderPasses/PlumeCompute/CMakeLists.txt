add_plugin(PlumeCompute)

target_sources(PlumeCompute PRIVATE
    PlumeCompute.cpp
    PlumeCompute.h
    PlumeCompute.cs.slang
    SmokeLayer.cs.slang
    Plume.cs.slang
)

target_source_group(PlumeCompute "RenderPasses")
