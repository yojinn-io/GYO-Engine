# Object_Connect's own tests, carried over from the original tests/ with the
# original harness (TestSupport.hpp). The suites that read the bundled level data
# from disk come back with the assets (porting inventory, section 5).
gyo_get_game_target(object_connect logic object_connect_logic)
add_executable(gyo_object_connect_logic_tests
    TestMain.cpp
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
if(MSVC)
    target_compile_options(gyo_object_connect_logic_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_object_connect_logic_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_connect_logic_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_connect.logic COMMAND gyo_object_connect_logic_tests)
set_tests_properties(object_connect.logic PROPERTIES LABELS "cpu" TIMEOUT 60
    PASS_REGULAR_EXPRESSION "All Object_Connect core tests passed")
