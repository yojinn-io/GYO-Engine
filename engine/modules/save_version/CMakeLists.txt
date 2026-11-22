# Save versions (docs/architecture/target/v2/restore-entry-declaration.zh-Hant.md, section 19; Design Invariants in
# docs/architecture/target/v2/save-version.zh-Hant.md): Policy, "the current save version and the oldest one taken",
# and the check a load calls. A library (config/modules.csv library_only=yes): no registration unit.
add_library(gyo_save_version STATIC
    src/SaveVersion.cpp
)
add_library(GYO::SaveVersion ALIAS gyo_save_version)

target_include_directories(gyo_save_version PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6). It depends on no other Module.
# OrchestrationError is the refusal's type (a prepare function returns it as it is).
target_link_libraries(gyo_save_version PUBLIC GYO::Base GYO::Orchestration)
gyo_link_registered_dependencies(save_version)
target_compile_features(gyo_save_version PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_save_version PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_save_version PRIVATE -Wall -Wextra -Wpedantic)
endif()
