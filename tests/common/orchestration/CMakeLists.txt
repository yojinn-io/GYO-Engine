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
foreach(group IN ITEMS O-1 O-2 O-3 O-5 O-8 O-9 O-10 O-E1 O-E2 O-E3 O-E6 O-E7 restore)
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

# Include order (defect found in segment 3): each public orchestration header
# first, then every public kernel header. The orchestration layer is the first
# GYO::Engine namespace; a kernel header that names Engine::Base unqualified
# inside GYO::Kernel would then look in GYO::Engine and fail to compile. Built
# with the tests; a compile error is the failure.
file(GLOB_RECURSE _orchestration_headers RELATIVE "${GYO_REPOSITORY_ROOT}/engine/orchestration/include"
    CONFIGURE_DEPENDS "${GYO_REPOSITORY_ROOT}/engine/orchestration/include/*.hpp")
file(GLOB_RECURSE _kernel_headers RELATIVE "${GYO_REPOSITORY_ROOT}/engine/kernel/include"
    CONFIGURE_DEPENDS "${GYO_REPOSITORY_ROOT}/engine/kernel/include/*.hpp")
list(SORT _orchestration_headers)
list(SORT _kernel_headers)
set(_kernel_includes "")
foreach(_header IN LISTS _kernel_headers)
    string(APPEND _kernel_includes "#include \"${_header}\"\n")
endforeach()
set(_order_sources "")
foreach(_header IN LISTS _orchestration_headers)
    string(MAKE_C_IDENTIFIER "${_header}" _id)
    set(_source "${CMAKE_CURRENT_BINARY_DIR}/include_order/${_id}.cpp")
    file(CONFIGURE OUTPUT "${_source}" CONTENT "// Generated: ${_header} first, then every kernel header.\n#include \"${_header}\"\n${_kernel_includes}" @ONLY)
    list(APPEND _order_sources "${_source}")
endforeach()
add_library(gyo_orchestration_include_order OBJECT ${_order_sources})
target_link_libraries(gyo_orchestration_include_order PRIVATE GYO::Orchestration)
