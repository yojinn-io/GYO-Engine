# Tests and guards of the Engine kernel
# (docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md,
# docs/architecture/plans/kernel/registry-schedule-declaration.zh-Hant.md).
# Nothing here may need an SDL component: the Linux core preset runs it too.

# The kernel test executable links only GYO::Kernel and gyo_test_support
# (GYO::Base and doctest), so a kernel header that needs anything else does not
# compile here. The zone C fixture is a synthetic (c) System file for the G-5
# export (spec J-17 kernel half); it is in the executable so its registration
# site is a real source path.
set(GYO_KERNEL_TEST_SOURCES
    TestMain.cpp
    SignatureTests.cpp
    RegistryTests.cpp
    ZoneRuleTests.cpp
    SchedulerTests.cpp
    ZoneCExportTests.cpp
    CoverageTests.cpp
    zone_c_fixture/SyntheticSimulation.cpp
    zone_c_fixture/AnotherSimulation.cpp
)
function(gyo_kernel_test_executable name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE GYO::Kernel gyo_test_support doctest::doctest)
    target_compile_definitions(${name} PRIVATE
        GYO_KERNEL_TEST_REPOSITORY_ROOT="${GYO_REPOSITORY_ROOT}"
        GYO_KERNEL_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
    # Same warning level as the math and entity store tests, so header code
    # that would warn in a consumer fails here first. doctest includes
    # <windows.h> on MSVC, so its min and max macros are turned off.
    if(MSVC)
        target_compile_options(${name} PRIVATE /W4 /permissive-)
        target_compile_definitions(${name} PRIVATE NOMINMAX)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
gyo_kernel_test_executable(gyo_kernel_tests ${GYO_KERNEL_TEST_SOURCES})

# Spec J-18 "a different link order gives the same ids": the same sources in the
# reverse order. Both executables check the same Registry fingerprint golden.
list(REVERSE GYO_KERNEL_TEST_SOURCES)
gyo_kernel_test_executable(gyo_kernel_tests_reversed ${GYO_KERNEL_TEST_SOURCES})

# One ctest per J entry of the registry-schedule batch (declaration section 2),
# so each appears by name in the L1 tests.xml (declaration judgement 2). The
# doctest cases of entry J-n are named "J-n: ...".
set(GYO_KERNEL_J_ENTRIES
    J-10 J-11 J-12 J-13 J-14 J-15 J-16 J-17 J-18 J-19 J-20 J-21 J-22 J-23 J-24 J-25
    J-27 J-28 J-37 J-38 J-39 J-40 J-41 J-44 J-47)
# A filter that matches no case would pass silently, so a run of zero cases fails.
foreach(entry IN LISTS GYO_KERNEL_J_ENTRIES)
    add_test(NAME kernel.${entry} COMMAND gyo_kernel_tests "--test-case=${entry}:*")
endforeach()
# Everything else in the executable (the Plan export and its dataflow ratio),
# plus the J-18 link-order run.
add_test(NAME kernel.others COMMAND gyo_kernel_tests "--test-case-exclude=J-*")
add_test(NAME kernel.J-18_link_order COMMAND gyo_kernel_tests_reversed "--test-case=J-18:*")
foreach(test IN LISTS GYO_KERNEL_J_ENTRIES ITEMS others J-18_link_order)
    set_tests_properties(kernel.${test} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
# kernel.J-17 writes the G-5 export of the zone C fixture; kernel.J-17_zone_scan reads it.
set_tests_properties(kernel.J-17 PROPERTIES FIXTURES_SETUP kernel_zone_c_export)

# Text guards, run on the repository as text so they give the same result on
# every platform; their self-tests run them on synthetic trees. -B: no
# bytecode is written into the source tree, so an edited guard is never run
# from a stale cache.
#   kernel.guard         spec J-9 (target/kernel DI-KS 3 and 15, target/naming
#                        DI 4, target/error-handling DI 2)
#   kernel.naming_guard  target/naming DI 1 (namespace and directory) and DI 3
#                        (no role suffix on type names)
#   kernel.J-17_zone_scan  the G-5 export of the zone C fixture (written by kernel.J-17), read by
#                        tests/common/zone_scan/zone_scan.py; the finding
#                        count must be the fixture's planted count
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME kernel.guard
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/kernel_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME kernel.guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_kernel_guard.py" -v)
add_test(NAME kernel.naming_guard
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/naming_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME kernel.naming_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_naming_guard.py" -v)
add_test(NAME kernel.J-17_zone_scan
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/zone_c_export_check.py"
        --root "${GYO_REPOSITORY_ROOT}" --list "${CMAKE_CURRENT_BINARY_DIR}/kernel_zone_c_sources.json")
set_tests_properties(kernel.J-17_zone_scan PROPERTIES FIXTURES_REQUIRED kernel_zone_c_export)
set_tests_properties(kernel.guard kernel.guard_selftest kernel.naming_guard kernel.naming_guard_selftest
    kernel.J-17_zone_scan PROPERTIES LABELS "cpu" TIMEOUT 60)

# Writable data symbols of the kernel library (registry-schedule declaration
# section 3 item 1, judgement 9): what kernel_guard's K4 cannot see (a
# namespace-scope variable without "static"). nm on Linux and macOS; Windows is
# a declared limit (MSVC has no nm; dumpbin's output has another shape).
if(NOT MSVC AND NOT CMAKE_NM)
    message(FATAL_ERROR "kernel.symbols: no nm found (CMAKE_NM); the writable-symbol guard would silently vanish")
endif()
if(NOT MSVC)
    add_test(NAME kernel.symbols
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/kernel_symbols.py"
            --nm "${CMAKE_NM}" --library "$<TARGET_FILE:gyo_kernel>" --require "TypeKey anchor")
    set_tests_properties(kernel.symbols PROPERTIES LABELS "cpu" TIMEOUT 60)
endif()
add_test(NAME kernel.symbols_selftest
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_kernel_symbols.py" -v)
set_tests_properties(kernel.symbols_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)

# BU-3 record executable (migration plan section 6): finish with 500 synthetic
# Systems. Not a ctest: shared-runner timings are noise; run it by hand in
# Release and keep its output under build/evidence/.
add_executable(gyo_kernel_finish_record FinishRecord.cpp)
target_link_libraries(gyo_kernel_finish_record PRIVATE GYO::Kernel)
if(MSVC)
    target_compile_options(gyo_kernel_finish_record PRIVATE /bigobj)
endif()

# Compile-failure samples (a) (spec J-10, migration plan U5; after the review
# also J-11, J-13, member functions and hooks): one source, built
# at configure time once per variant. The control must compile, so a failure of
# another variant comes from the sample, not from include paths or flags; each
# rejected variant must fail, and with its own static_assert marker, so a
# failure for any other reason is not counted as expected (AGENTS 14.2).
# NO_CACHE: the check runs on every configure; the sample and the kernel headers
# are configure dependencies, so editing one re-runs the check. Any mismatch
# stops the configure. CMake folds long FATAL_ERROR messages; mutation
# expectations use the first words only.
function(gyo_kernel_compile_fail_samples)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/SystemContractSample.cpp")
    # The include directories GYO::Kernel gives its users (kernel and base).
    set(includes "${GYO_REPOSITORY_ROOT}/engine/kernel/include;${GYO_REPOSITORY_ROOT}/engine/base/include")
    # try_compile does not inherit directory compile options; MSVC needs the
    # standard-conforming preprocessor that engine/base/Assert.hpp requires
    # (build/cmake/GyoBuild.cmake) and the conformance mode the kernel builds with.
    set(flags)
    if(MSVC)
        set(flags /Zc:preprocessor /permissive-)
    endif()
    file(GLOB kernel_headers "${GYO_REPOSITORY_ROOT}/engine/kernel/include/engine/kernel/*.hpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}" ${kernel_headers})

    string(TIMESTAMP start "%s%f")
    try_compile(accepted SOURCES "${source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
        COMPILE_DEFINITIONS -DGYO_KERNEL_SAMPLE_ACCEPTED ${flags}
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
        OUTPUT_VARIABLE accepted_output
        NO_CACHE)
    string(TIMESTAMP after_control "%s%f")
    math(EXPR control_ms "(${after_control} - ${start}) / 1000")
    if(NOT accepted)
        message(FATAL_ERROR "kernel.compile_fail_sample: the control did not compile, so a sample "
            "cannot tell its own failure from a broken setup:\n${accepted_output}")
    endif()
    set(summary "control compiled in ${control_ms} ms")
    foreach(variant IN ITEMS CAPTURE:GYO_KERNEL_REJECT_CAPTURE WORLD:GYO_KERNEL_REJECT_PARAMETER
                             RETURN:GYO_KERNEL_REJECT_RETURN SELF_CONFLICT:GYO_KERNEL_REJECT_SELF_CONFLICT
                             CONDITION_WRITE:GYO_KERNEL_REJECT_CONDITION_WRITE
                             MEMBER:GYO_KERNEL_REJECT_MEMBER_FUNCTION HOOK_PARAMETER:GYO_KERNEL_REJECT_PARAMETER)
        string(REPLACE ":" ";" parts "${variant}")
        list(GET parts 0 name)
        list(GET parts 1 marker)
        string(TIMESTAMP before "%s%f")
        try_compile(rejected SOURCES "${source}"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
            COMPILE_DEFINITIONS -DGYO_KERNEL_SAMPLE_${name} ${flags}
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
            OUTPUT_VARIABLE rejected_output
            NO_CACHE)
        string(TIMESTAMP after "%s%f")
        math(EXPR variant_ms "(${after} - ${before}) / 1000")
        if(rejected)
            message(FATAL_ERROR "kernel.compile_fail_sample: variant ${name} compiled; "
                "the System contract no longer rejects it")
        endif()
        string(FIND "${rejected_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "kernel.compile_fail_sample: variant ${name} failed without ${marker}, "
                "so the failure is not the expected one:\n${rejected_output}")
        endif()
        string(APPEND summary "; ${name} rejected with ${marker} in ${variant_ms} ms")
    endforeach()
    message(STATUS "kernel.compile_fail_sample (try_compile): ${summary}")
endfunction()
gyo_kernel_compile_fail_samples()

# Delta-1 of the migration plan: the kernel is a leaf directly above GYO::Base;
# its links, private and interface alike, are exactly GYO::Base.
# gyo_expect_links is defined with the time tests, which configure first.
# Checked once the whole project is configured, so an edge added later from
# another directory is seen too.
function(gyo_expect_kernel_links)
    gyo_expect_links(gyo_kernel GYO::Base)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_kernel_links)
