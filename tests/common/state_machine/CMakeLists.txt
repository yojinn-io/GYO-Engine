# Tests of the state machine (docs/architecture/target/v2/state-machine-declaration.zh-Hant.md, section 7).
# The behaviour target links only GYO::StateMachine (Design Invariant 8: it compiles without the time Module); the
# combination cases with the authority, rollback and time Modules are another target.
add_executable(gyo_state_machine_tests TestMain.cpp BehaviorTests.cpp)
target_link_libraries(gyo_state_machine_tests PRIVATE GYO::StateMachine doctest::doctest)
add_executable(gyo_state_machine_combination_tests TestMain.cpp CombinationTests.cpp)
target_link_libraries(gyo_state_machine_combination_tests PRIVATE GYO::StateMachine GYO::Authority GYO::Rollback
    GYO::WorldTime doctest::doctest)
foreach(target IN ITEMS gyo_state_machine_tests gyo_state_machine_combination_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /bigobj)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
add_test(NAME state_machine.behaviour COMMAND gyo_state_machine_tests "--test-case=behaviour:*")
add_test(NAME state_machine.combination COMMAND gyo_state_machine_combination_tests "--test-case=combination:*")
set_tests_properties(state_machine.behaviour state_machine.combination PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant 8: the state machine links the kernel and the orchestration layer, nothing else (no time Module).
function(gyo_expect_state_machine_links)
    gyo_expect_links(gyo_state_machine GYO::Kernel GYO::Orchestration)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_state_machine_links)
