# The default entity allotter (docs/architecture/target/v2/world-gen-declaration.zh-Hant.md, section 5.10;
# Design Invariants in docs/architecture/target/v2/entity-ids.zh-Hant.md): RangeAllotter and the registration unit
# EntityIdsModule that puts it into every World it joins.
add_library(gyo_entity_ids STATIC
    src/EntityIdsModule.cpp
)
add_library(GYO::EntityIds ALIAS gyo_entity_ids)

target_include_directories(gyo_entity_ids PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6). It depends on no other Module (EI-3).
target_link_libraries(gyo_entity_ids PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(entity_ids)
target_compile_features(gyo_entity_ids PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_entity_ids PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_entity_ids PRIVATE -Wall -Wextra -Wpedantic)
endif()
