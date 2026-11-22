# Tests of the rollback Module (docs/architecture/target/v2/authority-rollback-declaration.zh-Hant.md, sections 5
# and 9.3). Synthetic data only; no product.
add_executable(gyo_rollback_tests TestMain.cpp RegistrationsTests.cpp StrategyTests.cpp CheckTests.cpp)
target_include_directories(gyo_rollback_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_rollback_tests PRIVATE GYO::Rollback gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_rollback_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_rollback_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_rollback_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS "rollback registrations" "rollback strategy" "rollback checks")
    string(REPLACE " " "_" name "${group}")
    add_test(NAME rollback.${name} COMMAND gyo_rollback_tests "--test-case=${group}:*")
    set_tests_properties(rollback.${name} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
# The budget measurement of judgement 5 (authority-rollback declaration 5.4.5): run by hand on the user's Linux host
# next to floor_bench (build/evidence/gyo2-authority-rollback/), not a ctest.
add_executable(gyo_rollback_budget RollbackBudget.cpp)
target_link_libraries(gyo_rollback_budget PRIVATE GYO::Rollback)
if(MSVC)
    target_compile_options(gyo_rollback_budget PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_rollback_budget PRIVATE -Wall -Wextra -Wpedantic)
endif()

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant R-3 (docs/architecture/target/v2/rollback.zh-Hant.md): the rollback Module knows no authority,
# network or state machine. Its links, private and interface alike, are exactly these, so a link to any other Module,
# one registered later included, stops the configure. Checked once the whole project is configured, so an edge added
# later from another directory is seen too (as for gyo_time and gyo_world_time).
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_links gyo_rollback
    GYO::Base GYO::Kernel GYO::Orchestration GYO::WorldTime)
