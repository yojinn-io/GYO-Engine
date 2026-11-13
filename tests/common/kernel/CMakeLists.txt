# Tests and guards of the Engine kernel scaffold
# (docs/architecture/plans/kernel/kernel-scaffold-declaration.zh-Hant.md).
# Nothing here may need an SDL component: the Linux core preset runs it too.

# Smoke test and compile-failure sample (b). The executable links only
# GYO::Kernel and gyo_test_support (GYO::Base and doctest), so a kernel header
# that needs anything else does not compile here.
add_executable(gyo_kernel_tests KernelSmokeTests.cpp)
target_include_directories(gyo_kernel_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_kernel_tests PRIVATE GYO::Kernel gyo_test_support doctest::doctest)
# Same warning level as the math and entity store tests, so header code that
# would warn in a consumer fails here first. doctest includes <windows.h> on
# MSVC, so its min and max macros are turned off.
if(MSVC)
    target_compile_options(gyo_kernel_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_kernel_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_kernel_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME gyo_kernel_tests COMMAND gyo_kernel_tests)
set_tests_properties(gyo_kernel_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# Text guards, run on the repository as text so they give the same result on
# every platform; their self-tests run them on synthetic trees. -B: no
# bytecode is written into the source tree, so an edited guard is never run
# from a stale cache.
#   kernel.guard         spec J-9 (target/kernel DI-KS 3 and 15, target/naming
#                        DI 4, target/error-handling DI 2)
#   kernel.naming_guard  target/naming DI 1 (namespace and directory) and DI 3
#                        (no role suffix on type names)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME kernel.guard
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/kernel_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME kernel.guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_kernel_guard.py" -v)
add_test(NAME kernel.naming_guard
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/naming_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME kernel.naming_guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_naming_guard.py" -v)
set_tests_properties(kernel.guard kernel.guard_selftest kernel.naming_guard kernel.naming_guard_selftest
    PROPERTIES LABELS "cpu" TIMEOUT 60)

# Compile-failure sample (a) (spec J-10, migration plan U5): one source, built
# twice at configure time. The control must compile, so a failure of the other
# build comes from the sample, not from include paths or flags; the other must
# fail, and with the sample's own static_assert message, so a failure for any
# other reason is not counted as expected (AGENTS 14.2). NO_CACHE: the check runs
# on every configure, so a change to the sample is seen without clearing the
# cache, and the sample files are configure dependencies, so editing one
# re-runs the check on the next build. Any mismatch stops the configure.
function(gyo_kernel_compile_fail_sample)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/RejectsCapturingCallable.cpp")
    set(includes "${GYO_REPOSITORY_ROOT}/engine/kernel/include;${CMAKE_CURRENT_SOURCE_DIR}/compile_fail")
    set(marker "GYO_KERNEL_SAMPLE_REJECTED")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${source}" "${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/SampleContract.hpp")

    string(TIMESTAMP start "%s%f")
    try_compile(accepted SOURCES "${source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
        COMPILE_DEFINITIONS -DGYO_KERNEL_SAMPLE_ACCEPTED
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
        OUTPUT_VARIABLE accepted_output
        NO_CACHE)
    string(TIMESTAMP middle "%s%f")
    try_compile(rejected SOURCES "${source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
        OUTPUT_VARIABLE rejected_output
        NO_CACHE)
    string(TIMESTAMP end "%s%f")
    math(EXPR accepted_ms "(${middle} - ${start}) / 1000")
    math(EXPR rejected_ms "(${end} - ${middle}) / 1000")

    if(NOT accepted)
        message(FATAL_ERROR "kernel.compile_fail_sample: the control (a captureless callable) did not "
            "compile, so the sample cannot tell its own failure from a broken setup:\n${accepted_output}")
    endif()
    if(rejected)
        message(FATAL_ERROR "kernel.compile_fail_sample: a callable with state compiled; "
            "the static_assert of SampleContract.hpp no longer rejects it")
    endif()
    string(FIND "${rejected_output}" "${marker}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "kernel.compile_fail_sample: the rejected build failed without ${marker}, "
            "so the failure is not the expected one:\n${rejected_output}")
    endif()
    message(STATUS "kernel.compile_fail_sample (try_compile): control compiled in ${accepted_ms} ms; "
        "stateful callable rejected with ${marker} in ${rejected_ms} ms")
endfunction()
gyo_kernel_compile_fail_sample()

# Delta-1 of the migration plan: the kernel is a leaf directly above GYO::Base;
# its links, private and interface alike, are exactly GYO::Base.
# gyo_expect_links is defined with the time tests, which configure first.
# Checked once the whole project is configured, so an edge added later from
# another directory is seen too.
function(gyo_expect_kernel_links)
    gyo_expect_links(gyo_kernel GYO::Base)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_kernel_links)
