# Object_Connect's own tests, carried over from the original tests/ with the
# original harness (TestSupport.hpp). The bundled-data cases read
# assets/object_connect through BundledData.cpp.
gyo_get_game_target(object_connect logic object_connect_logic)
add_executable(gyo_object_connect_logic_tests
    TestMain.cpp
    BundledData.cpp
    data/PuzzleCatalogTests.cpp
    game/FinalResultsTests.cpp
    game/GameFlowTests.cpp
    game/MouseCursorStateTests.cpp
    game/MusicSequencePolicyTests.cpp
    geometry/Geometry2DTests.cpp
    puzzle/PuzzleBoardTests.cpp
    tentacle/BloodTentacleTests.cpp
    tentacle/RibbonStripTests.cpp)
target_include_directories(gyo_object_connect_logic_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gyo_object_connect_logic_tests PRIVATE ${object_connect_logic})
target_compile_definitions(gyo_object_connect_logic_tests PRIVATE
    OBJECT_CONNECT_TEST_RESOURCE_ROOT="${GYO_REPOSITORY_ROOT}/assets/object_connect")
if(MSVC)
    target_compile_options(gyo_object_connect_logic_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_object_connect_logic_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_connect_logic_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_connect.logic COMMAND gyo_object_connect_logic_tests)
set_tests_properties(object_connect.logic PROPERTIES LABELS "cpu" TIMEOUT 60
    PASS_REGULAR_EXPRESSION "All Object_Connect core tests passed")

# The game in a World, run headless frame by frame (doctest, as the Engine's tests).
gyo_get_game_target(object_connect world object_connect_world)
add_executable(gyo_object_connect_world_tests world/TestMain.cpp world/WorldTests.cpp)
target_link_libraries(gyo_object_connect_world_tests PRIVATE ${object_connect_world} doctest::doctest)
if(MSVC)
    target_compile_options(gyo_object_connect_world_tests PRIVATE /W4 /permissive- /utf-8 /bigobj)
    target_compile_definitions(gyo_object_connect_world_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_connect_world_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_connect.world COMMAND gyo_object_connect_world_tests "--test-case=world:*")
set_tests_properties(object_connect.world PROPERTIES LABELS "cpu" TIMEOUT 60
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
