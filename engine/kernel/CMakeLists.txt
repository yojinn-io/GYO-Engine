# Engine kernel (docs/architecture/target/kernel.zh-Hant.md): World, System
# contract, Registry, Scheduler and Executor. A leaf directly above GYO::Base,
# below every capability module, product and plugin (Delta-1 of
# docs/architecture/plans/kernel/migration-plan.zh-Hant.md). Header-only while
# it has no source file; the first batch that adds one makes it STATIC.
add_library(gyo_kernel INTERFACE)
add_library(GYO::Kernel ALIAS gyo_kernel)

target_include_directories(gyo_kernel INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_kernel INTERFACE GYO::Base)
target_compile_features(gyo_kernel INTERFACE cxx_std_20)
