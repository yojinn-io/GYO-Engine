# Tests of the logging Module (docs/architecture/target/v2/logging-declaration.zh-Hant.md, section 9).
# They see only the Module's public include directory and inject their own clock (section 9.1).
add_executable(gyo_logging_tests
    TestMain.cpp
    AppendTests.cpp
    FatalTests.cpp
    FileSinkTests.cpp
    FormatTests.cpp
    LoggerTests.cpp
)
# GYO::Json reads every JSON line back (Design Invariant LG-5).
find_package(Threads REQUIRED)
target_link_libraries(gyo_logging_tests PRIVATE GYO::Logging GYO::Json gyo_test_support doctest::doctest Threads::Threads)
# Files a test writes go to the build directory (trace declaration T6).
target_compile_definitions(gyo_logging_tests PRIVATE GYO_LOGGING_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(gyo_logging_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_logging_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_logging_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS logger builder format console file append fatal)
    add_test(NAME logging.${group} COMMAND gyo_logging_tests "--test-case=${group}:*")
    set_tests_properties(logging.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# Design Invariant LG-2: std::terminate writes one Fatal record of category "terminate", then flushes. The
# process aborts, so a probe executable runs under cmake -P and the script reads the file it leaves.
add_executable(gyo_logging_terminate_probe TerminateProbe.cpp)
target_link_libraries(gyo_logging_terminate_probe PRIVATE GYO::Logging)
foreach(mode IN ITEMS std other none)
    add_test(NAME logging.terminate.${mode}
        COMMAND ${CMAKE_COMMAND}
            -DPROBE=$<TARGET_FILE:gyo_logging_terminate_probe>
            -DMODE=${mode}
            -DFILE=${CMAKE_CURRENT_BINARY_DIR}/terminate-${mode}.jsonl
            -P ${CMAKE_CURRENT_SOURCE_DIR}/TerminateProbe.cmake)
    set_tests_properties(logging.terminate.${mode} PROPERTIES LABELS "cpu" TIMEOUT 30)
endforeach()

# Design Invariants LG-2 and LG-8 on standard error: what the fatal path and the no-Logger path write there.
add_executable(gyo_logging_console_probe ConsoleProbe.cpp)
target_link_libraries(gyo_logging_console_probe PRIVATE GYO::Logging)
foreach(mode IN ITEMS fatal nologger twice)
    add_test(NAME logging.console_probe.${mode}
        COMMAND ${CMAKE_COMMAND} -DMODE=${mode} -P ${CMAKE_CURRENT_SOURCE_DIR}/ConsoleProbe.cmake
            -- $<TARGET_FILE:gyo_logging_console_probe>)
    set_tests_properties(logging.console_probe.${mode} PROPERTIES LABELS "cpu" TIMEOUT 30)
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

# Design Invariants LG-4 and LG-10: the include scan of both logging Modules, set up after scenario segment 3
# passed (declaration 9.3; AGENTS §3 on when to set up guards that bind the way of implementing).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME logging.guard
    COMMAND Python3::Interpreter -I -B "${CMAKE_CURRENT_SOURCE_DIR}/logging_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME logging.guard_selftest
    COMMAND Python3::Interpreter -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_logging_guard.py" -v)
set_tests_properties(logging.guard logging.guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)
