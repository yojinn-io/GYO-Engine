# Game time in a World (docs/architecture/target/v2/time-events-declaration.zh-Hant.md, section 3;
# Design Invariants in docs/architecture/target/v2/world-time.zh-Hant.md): the time Resources, the
# accumulator, and (from step 4 of the batch) the registration unit WorldTimeModule with the System that
# advances time and the exclusive System that runs the fixed step. engine/time stays the time base only
# (v8-arch-gyo2-game-time-in-world-time).
add_library(gyo_world_time STATIC
    src/Sampling.cpp
    src/TimeDiscardedLog.cpp
    src/TimeTypes.cpp
    src/WorldTimeModule.cpp
)
add_library(GYO::WorldTime ALIAS gyo_world_time)

target_include_directories(gyo_world_time PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Engine-internal links come from engine/config/modules.csv (O-6).
gyo_link_registered_dependencies(world_time)
target_compile_features(gyo_world_time PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_world_time PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_world_time PRIVATE -Wall -Wextra -Wpedantic)
endif()
