# Kernel spike (v8-arch-kernel-spike): a standalone C++ prototype that answers
# two questions about contract reliability for the kernel line
# (docs/architecture/plans/kernel/mechanism.zh-Hant.md §1 item 2, §3.5):
#   1. Can a System's {reads, writes} be derived from its parameter types at
#      compile time, with no second hand-written declaration?
#   2. Do restricted handles plus a debug-build World access check stop
#      bypass access?
# Results: docs/architecture/plans/kernel/spike-report.zh-Hant.md.
#
# Not part of the GYO build graph: the root CMakeLists.txt does not add this
# directory, no GYO target depends on it, and it includes no GYO module. It
# reuses only the repository's pinned doctest (third_party/doctest).
#
#   cmake -S spikes/kernel -B build/target/_build/kernel-spike -G Ninja
#   cmake --build build/target/_build/kernel-spike
#   ctest --test-dir build/target/_build/kernel-spike --output-on-failure
#
# Offline, point FetchContent at an existing doctest checkout with
# -DFETCHCONTENT_SOURCE_DIR_DOCTEST=<path to doctest-src>.

cmake_minimum_required(VERSION 3.25)
project(GyoKernelSpike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()
find_package(Threads REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/doctest
                 ${CMAKE_CURRENT_BINARY_DIR}/third_party/doctest)

add_library(gyo_kernel_spike INTERFACE)
target_include_directories(gyo_kernel_spike INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(GYO_KERNEL_SPIKE_TEST_SOURCES
    tests/Main.cpp
    tests/RegistryWorldTests.cpp
    tests/AccessDerivationTests.cpp
    tests/AccessCheckTests.cpp
)

# The same sources twice: the debug target checks World access, the release
# target defines NDEBUG and leaves KERNEL_SPIKE_ACCESS_CHECKS to its NDEBUG
# default (off). Both are built in every configuration so one build shows both
# behaviours; KERNEL_SPIKE_TEST_EXPECT_CHECKS tells the tests which one they are.
function(gyo_kernel_spike_tests name)
    add_executable(${name} ${GYO_KERNEL_SPIKE_TEST_SOURCES})
    target_link_libraries(${name} PRIVATE gyo_kernel_spike doctest::doctest Threads::Threads)
    target_compile_definitions(${name} PRIVATE ${ARGN})
    target_compile_options(${name} PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive-;/Zc:preprocessor;/Zc:__cplusplus;/EHsc>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic>")
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS cpu)
endfunction()

gyo_kernel_spike_tests(gyo_kernel_spike_tests_debug KERNEL_SPIKE_ACCESS_CHECKS=1 KERNEL_SPIKE_TEST_EXPECT_CHECKS=1)
gyo_kernel_spike_tests(gyo_kernel_spike_tests_release NDEBUG KERNEL_SPIKE_TEST_EXPECT_CHECKS=0)
