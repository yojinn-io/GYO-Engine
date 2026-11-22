# Tests of game time (docs/architecture/target/v2/time-events-declaration.zh-Hant.md, section 6.4).
add_executable(gyo_world_time_tests TestMain.cpp TimeTypesTests.cpp WorldTimeModuleTests.cpp WorldGenTimeTests.cpp)
target_link_libraries(gyo_world_time_tests PRIVATE GYO::WorldTime gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_world_time_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_world_time_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_world_time_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS types module sampling fixed restore carry-over generation)
    add_test(NAME world_time.${group} COMMAND gyo_world_time_tests "--test-case=${group}:*")
    set_tests_properties(world_time.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
# The budget measurements of judgement 5 (time-events declaration 9.2, 13): run by hand on one host, not a ctest.
add_executable(gyo_world_time_budget TimeModuleBudget.cpp)
target_link_libraries(gyo_world_time_budget PRIVATE GYO::WorldTime)
if(MSVC)
    target_compile_options(gyo_world_time_budget PRIVATE /W4 /permissive- /bigobj)
    target_compile_definitions(gyo_world_time_budget PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_world_time_budget PRIVATE -Wall -Wextra -Wpedantic)
endif()

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant WT-4: world_time links the orchestration layer, the kernel, the time base, Base and logging,
# nothing else (v8-arch-gyo2-core-not-registered: the links to core, ecs and orchestration are written by hand).
function(gyo_expect_world_time_links)
    gyo_expect_links(gyo_world_time GYO::Base GYO::Kernel GYO::Logging GYO::Orchestration GYO::Time)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_world_time_links)
