# Tests of game time (docs/architecture/target/v2/time-events-declaration.zh-Hant.md, section 6.4).
add_executable(gyo_world_time_tests TestMain.cpp TimeTypesTests.cpp WorldTimeModuleTests.cpp)
target_link_libraries(gyo_world_time_tests PRIVATE GYO::WorldTime gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_world_time_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_world_time_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_world_time_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
# WT-4: world_time links exactly the modules its row of config/modules.csv names (added by
# v8-arch-gyo2-authority-rollback step 1, the guard time-events-declaration 6.3 deferred to batch 4). Checked once
# the whole project is configured, so an edge added later from another directory is seen too (as for gyo_time).
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_links gyo_world_time
    GYO::Base GYO::Kernel GYO::Logging GYO::Orchestration GYO::Time)
foreach(group IN ITEMS types module sampling fixed restore)
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
