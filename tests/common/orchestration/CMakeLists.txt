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
foreach(group IN ITEMS O-1 O-2 O-3 O-5 O-8 O-9 O-10 O-E1 O-E2 O-E3 O-E7 restore)
    add_test(NAME orchestration.${group} COMMAND gyo_orchestration_tests "--test-case=${group}:*")
    set_tests_properties(orchestration.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
