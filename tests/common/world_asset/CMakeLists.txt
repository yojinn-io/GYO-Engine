# Tests of world_asset (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md, section 10; step 3
# of section 9): the scenario test (scenario.*), what a World sees of the store (view.*), the hash functions in
# World::Summary (summary.*), and the compile-failure samples of the public contract (AS-10 (a), AS-11), built at
# configure time. The executors and test loaders are the store's (tests/common/asset/StoreSupport.hpp).
add_executable(gyo_world_asset_tests
    TestMain.cpp
    ScenarioTests.cpp
    ViewTests.cpp
    SummaryTests.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(gyo_world_asset_tests PRIVATE GYO::WorldAsset GYO::Threads gyo_test_support doctest::doctest Threads::Threads)
target_include_directories(gyo_world_asset_tests PRIVATE "${GYO_REPOSITORY_ROOT}/tests/common/asset")
target_compile_definitions(gyo_world_asset_tests PRIVATE GYO_ASSET_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(gyo_world_asset_tests PRIVATE /W4 /permissive- /bigobj)
    target_compile_definitions(gyo_world_asset_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_world_asset_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS scenario view summary)
    add_test(NAME world_asset.${group} COMMAND gyo_world_asset_tests "--test-case=${group}:*")
    set_tests_properties(world_asset.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

# Compile-failure samples (declaration section 10, "負向編譯的判法"): one source, built at configure time once per
# variant. The control must compile, so a failure elsewhere comes from the sample, not from include paths or
# flags. Each variant must fail, and with a static assertion's failure that names its marker on the same line: a
# marker alone is not enough, because a compiler prints the failing source line, and an unrelated hard error on
# the line of the static_assert would show the marker too (the kernel's precedent, tests/common/kernel/
# CMakeLists.txt, looks for the marker only; reported in the declaration's section 15 item 12).
# NO_CACHE: the check runs on every configure; the sample and the asset and world_asset headers are configure
# dependencies, so editing one runs it again. Any mismatch stops the configure.
function(gyo_world_asset_compile_fail_samples)
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/compile_fail/AssetContractSample.cpp")
    # The include directories GYO::WorldAsset gives the two headers the sample uses.
    set(includes "${GYO_REPOSITORY_ROOT}/engine/modules/world_asset/include;${GYO_REPOSITORY_ROOT}/engine/modules/asset/include;${GYO_REPOSITORY_ROOT}/engine/ecs/kernel/include;${GYO_REPOSITORY_ROOT}/engine/core/base/include")
    set(flags)
    if(MSVC)
        set(flags /Zc:preprocessor /permissive-)
    endif()
    file(GLOB headers
        "${GYO_REPOSITORY_ROOT}/engine/modules/world_asset/include/engine/world_asset/*.hpp"
        "${GYO_REPOSITORY_ROOT}/engine/modules/asset/include/engine/asset/*.hpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}" ${headers})

    try_compile(accepted SOURCES "${source}"
        CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
        COMPILE_DEFINITIONS -DGYO_ASSET_SAMPLE_ACCEPTED ${flags}
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
        OUTPUT_VARIABLE accepted_output
        NO_CACHE)
    if(NOT accepted)
        message(FATAL_ERROR "world_asset.compile_fail_sample: the control did not compile, so a sample "
            "cannot tell its own failure from a broken setup:\n${accepted_output}")
    endif()
    set(summary "control compiled")
    foreach(variant IN ITEMS HANDLE_STATE:GYO_ASSET_SAMPLE_HANDLE_HAS_NO_STATE
                             HANDLE_GET:GYO_ASSET_SAMPLE_HANDLE_HAS_NO_GET
                             HANDLE_BODY:GYO_ASSET_SAMPLE_HANDLE_BODY_IS_PRIVATE
                             HANDLE_LIFE:GYO_ASSET_SAMPLE_HANDLE_LIFE_IS_PRIVATE
                             REQUESTS_STATE:GYO_ASSET_SAMPLE_REQUESTS_HAVE_NO_STATE
                             REQUESTS_STORE:GYO_ASSET_SAMPLE_REQUESTS_GIVE_NO_STORE
                             VIEW_GET_WRITABLE:GYO_ASSET_SAMPLE_VIEW_GET_IS_READ_ONLY
                             VIEW_MUTATION:GYO_ASSET_SAMPLE_VIEW_HAS_NO_MUTATION
                             VIEW_COPY_ASSIGN:GYO_ASSET_SAMPLE_VIEW_IS_NOT_COPY_ASSIGNABLE
                             VIEW_CONSTRUCT:GYO_ASSET_SAMPLE_VIEW_IS_NOT_CONSTRUCTIBLE)
        string(REPLACE ":" ";" parts "${variant}")
        list(GET parts 0 name)
        list(GET parts 1 marker)
        try_compile(rejected SOURCES "${source}"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${includes}"
            COMPILE_DEFINITIONS -DGYO_ASSET_SAMPLE_${name} ${flags}
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
            OUTPUT_VARIABLE rejected_output
            NO_CACHE)
        if(rejected)
            message(FATAL_ERROR "world_asset.compile_fail_sample: variant ${name} compiled; "
                "the public contract no longer rejects it")
        endif()
        # A static assertion's failure (clang, gcc: "static assertion failed"; MSVC: "static_assert failed")
        # with the marker later on the same line.
        if(NOT rejected_output MATCHES "static[_ ]assert(ion)? failed[^\n]*${marker}")
            message(FATAL_ERROR "world_asset.compile_fail_sample: variant ${name} failed without a static "
                "assertion naming ${marker} on its line, so the failure is not the expected one:\n${rejected_output}")
        endif()
        string(APPEND summary "; ${name} rejected with ${marker}")
    endforeach()
    message(STATUS "world_asset.compile_fail_sample (try_compile): ${summary}")
endfunction()
gyo_world_asset_compile_fail_samples()
