# Engine kernel (docs/architecture/target/kernel.zh-Hant.md): World, System
# contract, Registry, Scheduler and Executor. A leaf directly above GYO::Base,
# below every capability module, product and plugin (Delta-1 of
# docs/architecture/plans/kernel/migration-plan.zh-Hant.md). STATIC since the
# registry-schedule batch added the first source files; it still links only
# GYO::Base. world-exec (Delta-3 of the migration plan) adds GYO::EntityStore:
# the World's columns are kept by engine/entity_store's algorithms
# (target/world.zh-Hant.md section 6, layout 2).
add_library(gyo_kernel STATIC
    src/Executor.cpp
    src/Fingerprint.cpp
    src/Registry.cpp
    src/Scheduler.cpp
    src/Verification.cpp
    src/World.cpp
)
add_library(GYO::Kernel ALIAS gyo_kernel)

target_include_directories(gyo_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_kernel PUBLIC GYO::Base GYO::EntityStore)
target_compile_features(gyo_kernel PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_kernel PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_kernel PRIVATE -Wall -Wextra -Wpedantic)
endif()
