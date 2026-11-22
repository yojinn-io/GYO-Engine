# Tests of the asset Module's library (declaration docs/architecture/target/v2/asset-declaration.zh-Hant.md,
# section 10; step 1 of section 9). The executable links GYO::Asset and nothing else of the Engine, so the
# direct calls of a loader show that a loader needs no ECS (Design Invariant AS-3).
# contract/ holds the catalog's examples (5.3, "範例集"; Design Invariant AS-12): contract/invalid/<rule>-<name>
# breaks one (A) rule each, contract/valid/<name> is content the rules deliberately accept, and
# contract/valid/master-object-fps-pvp is master's assets/object_fps_pvp content.json and asset_catalog.json
# (3b411e21) as they were.
add_executable(gyo_asset_tests
    TestMain.cpp
    CatalogTests.cpp
    ContentPathTests.cpp
    LoaderTests.cpp
    ReadWholeFileTests.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(gyo_asset_tests PRIVATE GYO::Asset doctest::doctest Threads::Threads)
target_compile_definitions(gyo_asset_tests PRIVATE
    GYO_ASSET_CONTRACT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/contract"
    GYO_ASSET_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(gyo_asset_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_asset_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_asset_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
foreach(group IN ITEMS catalog path read loader)
    add_test(NAME asset.${group} COMMAND gyo_asset_tests "--test-case=${group}:*")
    set_tests_properties(asset.${group} PROPERTIES LABELS "cpu" TIMEOUT 60
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()
