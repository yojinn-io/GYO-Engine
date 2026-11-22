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

# Include order (defect found in segment 3): each public orchestration header
# first, then every public kernel header. The orchestration layer was the first
# GYO::Engine namespace while the carried units still lived under the old root
# Engine::; a kernel header that named Engine::Base unqualified inside GYO::Kernel
# then looked in GYO::Engine and failed to compile. Since
# v8-arch-gyo2-namespace-root every unit is under GYO::Engine and that collision
# cannot happen; whether to keep these objects is a decision pending with the
# user (dev_log 2026_11_22_gyo2_namespace_root). Built with the tests; a compile
# error is the failure.
file(GLOB_RECURSE _orchestration_headers RELATIVE "${GYO_REPOSITORY_ROOT}/engine/orchestration/include"
    CONFIGURE_DEPENDS "${GYO_REPOSITORY_ROOT}/engine/orchestration/include/*.hpp")
file(GLOB_RECURSE _kernel_headers RELATIVE "${GYO_REPOSITORY_ROOT}/engine/ecs/kernel/include"
    CONFIGURE_DEPENDS "${GYO_REPOSITORY_ROOT}/engine/ecs/kernel/include/*.hpp")
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
# The first Engine Module in GYO::Engine that goes into a World, engine/world_time
# (time-events declaration 3.1): each of its public headers first, then every
# orchestration and kernel header; and each orchestration header first, then
# every world_time header (the order gyo_product_units generates). A header that
# named the old root Engine:: unqualified inside GYO::Engine::WorldTime failed here
# (see above: the collision is gone).
file(GLOB_RECURSE _world_time_headers RELATIVE "${GYO_REPOSITORY_ROOT}/engine/modules/world_time/include"
    CONFIGURE_DEPENDS "${GYO_REPOSITORY_ROOT}/engine/modules/world_time/include/*.hpp")
list(SORT _world_time_headers)
set(_orchestration_includes "")
foreach(_header IN LISTS _orchestration_headers)
    string(APPEND _orchestration_includes "#include \"${_header}\"\n")
endforeach()
set(_world_time_includes "")
foreach(_header IN LISTS _world_time_headers)
    string(APPEND _world_time_includes "#include \"${_header}\"\n")
endforeach()
foreach(_header IN LISTS _world_time_headers)
    string(MAKE_C_IDENTIFIER "${_header}" _id)
    set(_source "${CMAKE_CURRENT_BINARY_DIR}/include_order/${_id}.cpp")
    file(CONFIGURE OUTPUT "${_source}" CONTENT "// Generated: ${_header} first, then every orchestration and kernel header.\n#include \"${_header}\"\n${_orchestration_includes}${_kernel_includes}" @ONLY)
    list(APPEND _order_sources "${_source}")
endforeach()
foreach(_header IN LISTS _orchestration_headers)
    string(MAKE_C_IDENTIFIER "${_header}" _id)
    set(_source "${CMAKE_CURRENT_BINARY_DIR}/include_order/${_id}_then_world_time.cpp")
    file(CONFIGURE OUTPUT "${_source}" CONTENT "// Generated: ${_header} first, then every world_time header.\n#include \"${_header}\"\n${_world_time_includes}" @ONLY)
    list(APPEND _order_sources "${_source}")
endforeach()
add_library(gyo_orchestration_include_order OBJECT ${_order_sources})
target_link_libraries(gyo_orchestration_include_order PRIVATE GYO::Orchestration GYO::WorldTime)

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
# Checked once the whole project is configured, so an edge added later from
# another directory is seen too (as for gyo_time).
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_links gyo_orchestration GYO::Base GYO::Kernel)

# Error levels D2 (docs/architecture/target/v2/error-levels-declaration.zh-Hant.md section 9.2,
# [el-silent-orchestration]): the orchestration layer's error paths leave standard error empty
# (tests/common/support/SilentProbe.cmake); WarningCounter sees Base::Log only, this sees any write.
gyo_add_silent_probe(orchestration.el-silent gyo_orchestration_tests "${CMAKE_CURRENT_SOURCE_DIR}/silent_cases.txt")
