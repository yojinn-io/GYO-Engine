# Tests of the transform Module (docs/architecture/target/v2/transform-camera-declaration.zh-Hant.md, section 6.1;
# Design Invariants TF-1 to TF-5 in docs/architecture/target/v2/transform.zh-Hant.md). Synthetic units only; the
# rollback equivalence case (T-7) links the time and rollback Modules into the test, not into transform.
add_executable(gyo_transform_tests TestMain.cpp TransformTests.cpp)
target_link_libraries(gyo_transform_tests PRIVATE GYO::Transform GYO::EntityIds GYO::WorldTime GYO::Rollback
    gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_transform_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_transform_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_transform_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME transform.module COMMAND gyo_transform_tests "--test-case=transform:*")
set_tests_properties(transform.module PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant TF-4: transform links the orchestration layer, the kernel, Base and Math, nothing else.
function(gyo_expect_transform_links)
    gyo_expect_links(gyo_transform GYO::Base GYO::Kernel GYO::Math GYO::Orchestration)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_transform_links)
