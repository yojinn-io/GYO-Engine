# Tests of the logging Module (docs/architecture/target/v2/logging-declaration.zh-Hant.md, section 9).
# They see only the Module's public include directory and inject their own clock (section 9.1).
add_executable(gyo_logging_tests
    TestMain.cpp
    LoggerTests.cpp
)
target_link_libraries(gyo_logging_tests PRIVATE GYO::Logging gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_logging_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_logging_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_logging_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS logger builder)
    add_test(NAME logging.${group} COMMAND gyo_logging_tests "--test-case=${group}:*")
    set_tests_properties(logging.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# Design Invariant LG-7 replaces the global operator new for its executable, so it has one of its own.
add_executable(gyo_logging_allocation_tests TestMain.cpp WriteAllocationTests.cpp)
target_link_libraries(gyo_logging_allocation_tests PRIVATE GYO::Logging doctest::doctest)
if(MSVC)
    target_compile_options(gyo_logging_allocation_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_logging_allocation_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME logging.alloc COMMAND gyo_logging_allocation_tests)
set_tests_properties(logging.alloc PROPERTIES LABELS "cpu" TIMEOUT 60 FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

# Design Invariant LG-10: GYO::Base publicly, and GYO::Json, GYO::Os, GYO::Threads, GYO::Time
# privately; nothing else, and none of the private four handed to the Module's users. Checked once the
# whole project is configured, so an edge added later from another directory is seen too
# (gyo_expect_links comes from tests/common/time).
function(gyo_expect_logging_links)
    gyo_expect_links(gyo_logging GYO::Base GYO::Json GYO::Os GYO::Threads GYO::Time)
    get_target_property(interface gyo_logging INTERFACE_LINK_LIBRARIES)
    foreach(private IN ITEMS GYO::Json GYO::Os GYO::Threads GYO::Time)
        if(private IN_LIST interface)
            message(FATAL_ERROR "gyo_logging exposes ${private} to its users; link it PRIVATE")
        endif()
    endforeach()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_logging_links)
