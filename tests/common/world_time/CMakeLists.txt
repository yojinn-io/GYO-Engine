# Tests of game time (docs/architecture/target/v2/time-events-declaration.zh-Hant.md, section 6.4).
add_executable(gyo_world_time_tests TestMain.cpp TimeTypesTests.cpp WorldTimeModuleTests.cpp)
target_link_libraries(gyo_world_time_tests PRIVATE GYO::WorldTime gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_world_time_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_world_time_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_world_time_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS types module sampling fixed restore)
    add_test(NAME world_time.${group} COMMAND gyo_world_time_tests "--test-case=${group}:*")
    set_tests_properties(world_time.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
