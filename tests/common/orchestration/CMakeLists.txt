# Tests of the orchestration layer (docs/architecture/target/v2/orchestration-skeleton-declaration.zh-Hant.md,
# sections 7.3 and 11). Synthetic units and register rows only; no product.
# Behaviour tests only: the scans that bind the implementation wait until
# scenario segment 1 passes (declaration 7.4, v8-arch-gyo2-guards-after-mechanism-stable).
add_executable(gyo_orchestration_tests
    TestMain.cpp
    LifecycleTests.cpp
    GameInitTests.cpp
    ReplaceTests.cpp
    InsertTests.cpp
    RestoreFlowTests.cpp
    DefectTests.cpp
    CoverageTests.cpp
    SkeletonTests.cpp
    TraceSpanTests.cpp
    ModuleBufferTests.cpp
)
target_include_directories(gyo_orchestration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gyo_orchestration_tests PRIVATE GYO::Orchestration gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_orchestration_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_orchestration_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_orchestration_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# One ctest per group (the doctest cases are named "<group>: ..."); a run of
# zero cases fails.
foreach(group IN ITEMS O-1 O-2 O-3 O-4 O-5 O-8 O-9 O-10 O-E1 O-E2 O-E3 O-E6 O-E7 restore trace-spans module-buffer)
    add_test(NAME orchestration.${group} COMMAND gyo_orchestration_tests "--test-case=${group}:*")
    set_tests_properties(orchestration.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# The table gyo_product_units generates (declaration 3.2 point 3), from the
# synthetic registers in units/: built and given to Product::Build.
add_library(gyo_orchestration_test_units INTERFACE)
target_include_directories(gyo_orchestration_test_units INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/units)
target_link_libraries(gyo_orchestration_test_units INTERFACE GYO::Orchestration)
add_executable(gyo_orchestration_units_tests TestMain.cpp units/UnitsTests.cpp)
gyo_product_units(gyo_orchestration_units_tests DISABLE_MODULES off MODULES extra PLUGINS tool
    REGISTERS "${CMAKE_CURRENT_SOURCE_DIR}/units/modules.csv" "${CMAKE_CURRENT_SOURCE_DIR}/units/plugins.csv")
target_link_libraries(gyo_orchestration_units_tests PRIVATE gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_orchestration_units_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_orchestration_units_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_orchestration_units_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME orchestration.units COMMAND gyo_orchestration_units_tests "--test-case=units:*")
set_tests_properties(orchestration.units PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

# The scans the skeleton declaration deferred (section 7.4), stood up by
# v8-arch-gyo2-time-events step 1 once scenario segment 1 had passed:
#   orchestration.guard  O-3 (no Plan step read, no System called), O-4 (the
#                        include allowlist), O-6 (no hand-written engine link)
# and the link set of the layer (O-4: it depends on the kernel and Base only).
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME orchestration.guard
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/orchestration_guard.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME orchestration.guard_selftest
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}" -p "test_orchestration_guard.py" -v)
set_tests_properties(orchestration.guard orchestration.guard_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)
include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Checked once the whole project is configured, so an edge added later from
# another directory is seen too (as for gyo_time).
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_links gyo_orchestration GYO::Base GYO::Kernel)

# Error levels D2 (docs/architecture/target/v2/error-levels-declaration.zh-Hant.md section 9.2,
# [el-silent-orchestration]): the orchestration layer's error paths leave standard error empty
# (tests/common/support/SilentProbe.cmake); WarningCounter sees Base::Log only, this sees any write.
gyo_add_silent_probe(orchestration.el-silent gyo_orchestration_tests "${CMAKE_CURRENT_SOURCE_DIR}/silent_cases.txt")
