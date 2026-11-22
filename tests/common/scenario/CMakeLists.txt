# Scenario tests (declaration docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
# section 8; v8-arch-gyo2-scenario-tests): one synthetic game written against
# the new contracts goes through the mechanisms; each batch adds a segment.
# Tests only: no screen, no input, not a product, no old game's code.
add_executable(gyo_scenario_tests TestMain.cpp ScenarioTests.cpp ScenarioSegment2Tests.cpp ScenarioSegment3Tests.cpp ScenarioSegment4Tests.cpp
    ScenarioSegment5Tests.cpp ScenarioSegment6Tests.cpp)
target_include_directories(gyo_scenario_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_scenario_tests PRIVATE GYO::Orchestration GYO::EntityIds GYO::WorldTime GYO::WorldLogging GYO::Logging GYO::Trace GYO::Json
    GYO::Rollback GYO::Authority
    gyo_test_support doctest::doctest)
# Files a test writes go to the build directory (trace declaration T6).
target_compile_definitions(gyo_scenario_tests PRIVATE GYO_SCENARIO_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(gyo_scenario_tests PRIVATE /W4 /permissive- /bigobj)
    target_compile_definitions(gyo_scenario_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_scenario_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
# One ctest per segment; a run of zero cases fails.
add_test(NAME scenario.segment1 COMMAND gyo_scenario_tests "--test-case=scenario segment 1:*")
set_tests_properties(scenario.segment1 PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
add_test(NAME scenario.segment2 COMMAND gyo_scenario_tests "--test-case=scenario segment 2:*")
set_tests_properties(scenario.segment2 PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
add_test(NAME scenario.segment3 COMMAND gyo_scenario_tests "--test-case=scenario segment 3:*")
set_tests_properties(scenario.segment3 PROPERTIES LABELS "cpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
add_test(NAME scenario.segment4 COMMAND gyo_scenario_tests "--test-case=scenario segment 4:*")
set_tests_properties(scenario.segment4 PROPERTIES LABELS "cpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
add_test(NAME scenario.segment6 COMMAND gyo_scenario_tests "--test-case=scenario segment 6:*")
set_tests_properties(scenario.segment6 PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
add_test(NAME scenario.segment5 COMMAND gyo_scenario_tests "--test-case=scenario segment 5:*")
set_tests_properties(scenario.segment5 PROPERTIES LABELS "cpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
