# Orchestration layer (docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md;
# Design Invariants in docs/architecture/target/v2/orchestration.zh-Hant.md):
# registration units (Module, Plugin) and their lifecycle, the Product (a group
# of Worlds), the game initialization and frame lists, the runner, prepare
# functions, World replacement and dynamic insertion. Above the kernel; depends
# only on the kernel and Base (O-4), as its row in config/modules.csv says.
add_library(gyo_orchestration STATIC
    src/OrchestrationError.cpp
    src/Product.cpp
    src/Setup.cpp
)
add_library(GYO::Orchestration ALIAS gyo_orchestration)

target_include_directories(gyo_orchestration PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Engine-internal links are written here: core, ecs and orchestration are not
# Modules and are not registered (O-6 covers the Modules and Plugins only).
target_link_libraries(gyo_orchestration PUBLIC GYO::Kernel GYO::Base)
target_compile_features(gyo_orchestration PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_orchestration PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_orchestration PRIVATE -Wall -Wextra -Wpedantic)
endif()
