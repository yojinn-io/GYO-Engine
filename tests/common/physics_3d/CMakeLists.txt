# Tests of the physics_3d Module (docs/architecture/target/v2/physics-3d-declaration.zh-Hant.md, section 6; Design
# Invariants P3-1 to P3-6, section 8). Synthetic units only: the pure queries (queries.*), BoxSet equal to a linear
# scan (box_set.*), the literal binary32 bit patterns every platform must give (golden.*), and the World side
# (world.*); the rollback case links the time and rollback Modules into the test, not into physics_3d.
add_executable(gyo_physics_3d_tests
    TestMain.cpp
    QueryTests.cpp
    BoxSetTests.cpp
    GoldenTests.cpp
    WorldTests.cpp
)
target_link_libraries(gyo_physics_3d_tests PRIVATE GYO::Physics3d GYO::Transform GYO::EntityIds GYO::WorldTime
    GYO::Rollback gyo_test_support doctest::doctest)
if(MSVC)
    target_compile_options(gyo_physics_3d_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_physics_3d_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_physics_3d_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS queries box_set golden world)
    add_test(NAME physics_3d.${group} COMMAND gyo_physics_3d_tests "--test-case=${group}:*")
    set_tests_properties(physics_3d.${group} PROPERTIES LABELS "cpu" TIMEOUT 120
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectLinks.cmake")
# Design Invariant P3-6: physics_3d links the orchestration layer, the kernel, Base, Math and transform, nothing else.
function(gyo_expect_physics_3d_links)
    gyo_expect_links(gyo_physics_3d GYO::Base GYO::Kernel GYO::Math GYO::Orchestration GYO::Transform)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_physics_3d_links)

# Compile-failure sample (section 4.5): a spatial query whose Query lacks Without<Kernel::ChildOf> does not compile.
# One source, built at configure time per variant; the control must compile, and the variant must fail with a
# static assertion's failure that names its marker (tests/common/support/ExpectStaticAssert.cmake). NO_CACHE: runs
# on every configure; the sample and the physics_3d headers are configure dependencies.
include("${GYO_REPOSITORY_ROOT}/tests/common/support/ExpectStaticAssert.cmake")
function(gyo_physics_3d_compile_fail_samples)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/SpatialQuerySample.cpp")
    # The include directories GYO::Physics3d gives the headers the sample uses.
    set(includes "${GYO_REPOSITORY_ROOT}/engine/modules/physics_3d/include;${GYO_REPOSITORY_ROOT}/engine/modules/transform/include;${GYO_REPOSITORY_ROOT}/engine/ecs/kernel/include;${GYO_REPOSITORY_ROOT}/engine/ecs/entity_store/include;${GYO_REPOSITORY_ROOT}/engine/core/base/include;${GYO_REPOSITORY_ROOT}/engine/core/math/include")
    set(flags)
    if(MSVC)
        set(flags /Zc:preprocessor /permissive-)
    endif()
    file(GLOB headers "${GYO_REPOSITORY_ROOT}/engine/modules/physics_3d/include/engine/physics_3d/*.hpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}" ${headers})
    try_compile(accepted SOURCES "${source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
        COMPILE_DEFINITIONS -DGYO_PHYSICS3D_SAMPLE_ACCEPTED ${flags}
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
        OUTPUT_VARIABLE accepted_output
        NO_CACHE)
    if(NOT accepted)
        message(FATAL_ERROR "physics_3d.compile_fail_sample: the control did not compile, so a sample "
            "cannot tell its own failure from a broken setup:\n${accepted_output}")
    endif()
    set(summary "control compiled")
    foreach(variant IN ITEMS NO_WITHOUT_CHILD_OF:GYO_PHYSICS3D_REQUIRE_WITHOUT_CHILD_OF)
        string(REPLACE ":" ";" parts "${variant}")
        list(GET parts 0 name)
        list(GET parts 1 marker)
        try_compile(rejected SOURCES "${source}"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
            COMPILE_DEFINITIONS -DGYO_PHYSICS3D_SAMPLE_${name} ${flags}
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
            OUTPUT_VARIABLE rejected_output
            NO_CACHE)
        if(rejected)
            message(FATAL_ERROR "physics_3d.compile_fail_sample: variant ${name} compiled; "
                "a spatial query no longer requires Without<Kernel::ChildOf>")
        endif()
        gyo_expect_static_assert_failure(physics_3d.compile_fail_sample ${name} ${marker} "${rejected_output}")
        string(APPEND summary "; ${name} rejected with ${marker}")
    endforeach()
    message(STATUS "physics_3d.compile_fail_sample (try_compile): ${summary}")
endfunction()
gyo_physics_3d_compile_fail_samples()
