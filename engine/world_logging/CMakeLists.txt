# The World end of logging (docs/architecture/target/v2/logging-declaration.zh-Hant.md, sections 5.3 to
# 5.6; Design Invariants in docs/architecture/target/v2/logging.zh-Hant.md): Forward (a Message of a healthy
# World becomes records), TakeKernelDiagnostics (the runner takes the Kernel's diagnostics records) and, from
# step 4 of the batch, WriteFailure (the fatal path's last write of a WorldFailure). engine/logging stays free
# of ECS, as engine/time and engine/world_time are split.
add_library(gyo_world_logging STATIC
    src/Forward.cpp
    src/TakeKernelDiagnostics.cpp
)
add_library(GYO::WorldLogging ALIAS gyo_world_logging)

target_include_directories(gyo_world_logging PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_world_logging PUBLIC cxx_std_20)
# Engine-internal links come from engine/config/modules.csv (O-6).
gyo_link_registered_dependencies(world_logging)
if(MSVC)
    target_compile_options(gyo_world_logging PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_world_logging PRIVATE -Wall -Wextra -Wpedantic)
endif()
