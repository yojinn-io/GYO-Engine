# The state machine (state machine declaration docs/architecture/target/v2/state-machine-declaration.zh-Hant.md,
# section 5.2; Design Invariants in docs/architecture/target/v2/state-machine.zh-Hant.md): a template library the
# product's own build uses. It has no registration unit: the state enums are the product's.
add_library(gyo_state_machine STATIC
    src/Names.cpp
)
add_library(GYO::StateMachine ALIAS gyo_state_machine)

target_include_directories(gyo_state_machine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_state_machine PUBLIC cxx_std_20)
# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6). DI 8: no time Module.
target_link_libraries(gyo_state_machine PUBLIC GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(state_machine)
if(MSVC)
    target_compile_options(gyo_state_machine PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_state_machine PRIVATE -Wall -Wextra -Wpedantic)
endif()
