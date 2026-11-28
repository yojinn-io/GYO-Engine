# Tests of the OS layer (docs/architecture/os.md). This is the one common test
# directory allowed to assert per OS; the boundary guard does not scan it.
add_executable(gyo_os_tests OSErrorTests.cpp FilesTests.cpp FileWriterTests.cpp FileChangeTests.cpp
    ProcessTests.cpp MachineTests.cpp ConsoleTests.cpp ThreadTests.cpp WakeSignalTests.cpp DatagramSocketTests.cpp
    StreamSocketTests.cpp CaseTiming.cpp)
# GYO::Threads: ThreadTests reads a role's priority back on the role's thread.
target_link_libraries(gyo_os_tests PRIVATE GYO::OS GYO::Threads gyo_test_support doctest::doctest)
add_test(NAME gyo_os_tests COMMAND gyo_os_tests)
# A loopback TCP listener for tests of StreamSocket and what is built on it (the http Module, a game's calls to its
# services): the header is neutral, the native socket code stays here in tests/common/os.
add_library(gyo_test_loopback_listener STATIC LoopbackListener.cpp)
target_include_directories(gyo_test_loopback_listener PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/listener_include")
target_link_libraries(gyo_test_loopback_listener PUBLIC GYO::OS)
if(WIN32)
    target_link_libraries(gyo_test_loopback_listener PRIVATE ws2_32)
    target_compile_definitions(gyo_test_loopback_listener PRIVATE NOMINMAX)
endif()
target_link_libraries(gyo_os_tests PRIVATE gyo_test_loopback_listener)
set_tests_properties(gyo_os_tests PROPERTIES LABELS "cpu" TIMEOUT 60)
# CaseTiming.cpp (v8-arch-gyo2-os-test-timeout) times each case and keeps the
# log of a failed, slow or killed run here; it changes no check.
set(gyo_os_case_timing_dir "${CMAKE_CURRENT_BINARY_DIR}/case-timing")
target_compile_definitions(gyo_os_tests PRIVATE GYO_OS_CASE_TIMING_DIR="${gyo_os_case_timing_dir}")

# The wait fallback (src/sys/linux/WakeSignal.cpp) uses the standard library only,
# so it also builds on macOS: the same WakeSignalTests run against it here, and
# its mutants are killed on a developer's Mac too. It does not link gyo_os, so
# no executable holds two definitions of WakeSignal::Impl.
if(NOT WIN32)
    add_executable(gyo_os_wake_fallback_tests WakeSignalTests.cpp CaseTiming.cpp
        "${GYO_REPOSITORY_ROOT}/engine/core/os/src/sys/linux/WakeSignal.cpp")
    target_include_directories(gyo_os_wake_fallback_tests PRIVATE "${GYO_REPOSITORY_ROOT}/engine/core/os/include")
    target_compile_definitions(gyo_os_wake_fallback_tests PRIVATE
        GYO_WAKE_SIGNAL_FALLBACK=1 DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
        GYO_OS_CASE_TIMING_DIR="${gyo_os_case_timing_dir}")
    target_compile_features(gyo_os_wake_fallback_tests PRIVATE cxx_std_20)
    find_package(Threads REQUIRED)
    target_link_libraries(gyo_os_wake_fallback_tests PRIVATE GYO::Base Threads::Threads gyo_test_support
        doctest::doctest)
    add_test(NAME os.wake_fallback COMMAND gyo_os_wake_fallback_tests)
    set_tests_properties(os.wake_fallback PROPERTIES LABELS "cpu" TIMEOUT 60)
endif()

# Design Invariant 4 (Windows side): the public headers reach no Windows header
# and define no min or max. A separate executable, since the check must run
# before doctest's own windows.h.
add_executable(gyo_os_header_tests OSHeaderHygieneTests.cpp)
target_link_libraries(gyo_os_header_tests PRIVATE GYO::OS doctest::doctest)
target_compile_definitions(gyo_os_header_tests PRIVATE
    GYO_OS_INCLUDE_DIR="${GYO_REPOSITORY_ROOT}/engine/core/os/include"
    GYO_OS_HYGIENE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/OSHeaderHygieneTests.cpp")
add_test(NAME os.header_hygiene COMMAND gyo_os_header_tests)
set_tests_properties(os.header_hygiene PROPERTIES LABELS "cpu" TIMEOUT 60)

# The boundary guard scans the sources as text, so it gives the same result on
# every platform; its self-test runs it on synthetic trees.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME os.boundary_guard
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/os_boundary_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME os.boundary_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_*.py" -v)
set_tests_properties(os.boundary_guard os.boundary_guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)

# Console (command line, shutdown requests) needs a child process: the probe
# is started by console_check.py, which exits 77 for a check this runner cannot
# run (CTest shows it as skipped, so a gap stays visible).
add_executable(gyo_os_console_probe ConsoleProbe.cpp)
target_link_libraries(gyo_os_console_probe PRIVATE GYO::OS)
set(gyo_os_console_checks arguments shutdown)
if(WIN32)
    list(APPEND gyo_os_console_checks ctrl-c close)
endif()
foreach(check IN LISTS gyo_os_console_checks)
    string(REPLACE "-" "_" name "${check}")
    add_test(NAME os.console_${name}
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/console_check.py"
            --probe "$<TARGET_FILE:gyo_os_console_probe>" --check ${check})
    set_tests_properties(os.console_${name} PROPERTIES LABELS "cpu" TIMEOUT 90 SKIP_RETURN_CODE 77)
endforeach()

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# The OS layer is the lowest layer above GYO::Base: its links, private and
# interface alike, are exactly GYO::Base and OS system libraries (Design
# Invariant 3): on Windows shell32 for CommandLineToArgvW (D49) and
# SHGetFolderPathW (v8-arch-gyo2-os-user-data) and ws2_32 for DatagramSocket
# (v8-arch-gyo2-net-module step 0), elsewhere the thread library for
# pthread_setname_np (D69) and the wait fallback's mutex and condition variable
# (v8-arch-gyo2-os-wait).
# Checked once the whole project is configured, so an edge added later from
# another directory is seen too.
function(gyo_expect_os_links)
    if(WIN32)
        gyo_expect_links(gyo_os GYO::Base shell32 ws2_32)
    else()
        gyo_expect_links(gyo_os GYO::Base Threads::Threads)
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_os_links)

# Design Invariant 1: no target outside engine/os gets an OS-identifying compile
# definition from CMake, checked once the project is configured (the check is a
# module so cmake.ProcessCodePage can run it on synthetic trees).
include("${CMAKE_CURRENT_SOURCE_DIR}/OsDefinitionCheck.cmake")
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_no_os_definitions)

# Design Invariant 11: every executable target under the scanned roots carries
# the UTF-8 code page manifest, checked once the project is configured (the
# check is a module so cmake.ProcessCodePage can run it on synthetic trees).
include("${CMAKE_CURRENT_SOURCE_DIR}/ProcessCodePageCheck.cmake")
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_process_code_pages)
