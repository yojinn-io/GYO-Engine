# Tests of the World end of logging (docs/architecture/target/v2/logging-declaration.zh-Hant.md, section 9).
add_executable(gyo_world_logging_tests
    TestMain.cpp
    DiagnosticsTests.cpp
    ForwardTests.cpp
)
target_link_libraries(gyo_world_logging_tests PRIVATE GYO::WorldLogging GYO::WorldTime gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_world_logging_tests PRIVATE /W4 /permissive- /bigobj)
    target_compile_definitions(gyo_world_logging_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_world_logging_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS forward diagnostics)
    add_test(NAME world_logging.${group} COMMAND gyo_world_logging_tests "--test-case=${group}:*")
    set_tests_properties(world_logging.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# Design Invariant LG-10: GYO::Logging, GYO::Kernel and GYO::Orchestration, nothing else (gyo_expect_links
# comes from tests/common/time).
function(gyo_expect_world_logging_links)
    gyo_expect_links(gyo_world_logging GYO::Logging GYO::Kernel GYO::Orchestration)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_world_logging_links)

# Design Invariant LG-3 at compile time: the System Forward registers only reads, and a descriptor has what
# Forward needs. Each variant of compile_fail/ForwardSample.cpp must fail with its own marker; the control
# must compile (as the kernel's compile_fail samples).
function(gyo_world_logging_compile_fail_samples)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/ForwardSample.cpp")
    set(includes "")
    foreach(unit IN ITEMS world_logging logging kernel orchestration base entity_store)
        # Each unit's public include directory, from its target.
        get_target_property(unit_includes gyo_${unit} INTERFACE_INCLUDE_DIRECTORIES)
        list(APPEND includes ${unit_includes})
    endforeach()
    set(flags)
    if(MSVC)
        set(flags /Zc:preprocessor /permissive-)
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}"
        "${GYO_REPOSITORY_ROOT}/engine/modules/world_logging/include/engine/world_logging/Forward.hpp")
    try_compile(accepted SOURCES "${source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
        COMPILE_DEFINITIONS -DGYO_WORLD_LOGGING_SAMPLE_ACCEPTED ${flags}
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
        OUTPUT_VARIABLE accepted_output
        NO_CACHE)
    if(NOT accepted)
        message(FATAL_ERROR "world_logging.compile_fail_sample: the control did not compile, so a sample "
            "cannot tell its own failure from a broken setup:\n${accepted_output}")
    endif()
    set(summary "control compiled")
    foreach(variant IN ITEMS WRITE:GYO_WORLD_LOGGING_REJECT_WRITE DESCRIPTOR:GYO_WORLD_LOGGING_REJECT_DESCRIPTOR)
        string(REPLACE ":" ";" parts "${variant}")
        list(GET parts 0 name)
        list(GET parts 1 marker)
        try_compile(rejected SOURCES "${source}"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
            COMPILE_DEFINITIONS -DGYO_WORLD_LOGGING_SAMPLE_${name} ${flags}
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
            OUTPUT_VARIABLE rejected_output
            NO_CACHE)
        if(rejected)
            message(FATAL_ERROR "world_logging.compile_fail_sample: variant ${name} compiled; Forward no longer rejects it")
        endif()
        string(FIND "${rejected_output}" "${marker}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "world_logging.compile_fail_sample: variant ${name} failed without ${marker}, "
                "so the failure is not the expected one:\n${rejected_output}")
        endif()
        string(APPEND summary "; ${name} rejected with ${marker}")
    endforeach()
    message(STATUS "world_logging.compile_fail_sample (try_compile): ${summary}")
endfunction()
gyo_world_logging_compile_fail_samples()

# Judgement 7 (declaration section 10; measured only, no threshold): run by hand on one host, Release; not a ctest.
add_executable(gyo_logging_budget LoggingBudget.cpp)
target_link_libraries(gyo_logging_budget PRIVATE GYO::WorldLogging GYO::Logging GYO::Time)
if(MSVC)
    target_compile_options(gyo_logging_budget PRIVATE /W4 /permissive- /bigobj)
    target_compile_definitions(gyo_logging_budget PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_logging_budget PRIVATE -Wall -Wextra -Wpedantic)
endif()
