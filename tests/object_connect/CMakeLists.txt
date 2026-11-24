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
    input/PointerWrapTests.cpp
    puzzle/PuzzleBoardTests.cpp
    tentacle/BloodTentacleTests.cpp
    tentacle/RibbonStripTests.cpp
    ui/MenuLayoutTests.cpp)
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

# The game with its runtime, headless: the content read from assets/object_connect through the asset Module, the
# input and window Messages written by a test System where sdl_window writes them.
gyo_get_game_target(object_connect runtime object_connect_runtime)
add_executable(gyo_object_connect_runtime_tests runtime/TestMain.cpp runtime/RuntimeTests.cpp)
target_link_libraries(gyo_object_connect_runtime_tests PRIVATE ${object_connect_runtime} GYO::Threads doctest::doctest)
gyo_product_units(gyo_object_connect_runtime_tests MODULES render_world)
target_compile_definitions(gyo_object_connect_runtime_tests PRIVATE
    OBJECT_CONNECT_CONTENT_ROOT="${GYO_REPOSITORY_ROOT}/assets/object_connect")
if(MSVC)
    target_compile_options(gyo_object_connect_runtime_tests PRIVATE /W4 /permissive- /utf-8 /bigobj)
    target_compile_definitions(gyo_object_connect_runtime_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_connect_runtime_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_connect.runtime COMMAND gyo_object_connect_runtime_tests "--test-case=runtime:*")
set_tests_properties(object_connect.runtime PROPERTIES LABELS "cpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

# The program itself: the GPU device and the window open, the content loads, 30 frames are drawn and shown, and the
# program closes its window and exits with 0 (with --frames it exits with 1 when no frame was shown or a draw
# failed). Label gpu (sdl_gpu needs a device; SDL's dummy video driver has none): L1 runs it on Linux on Lavapipe
# under Xvfb, a machine with a GPU elsewhere. The window is not hidden: a hidden window gets no picture under X11
# (every frame skipped).
gyo_get_game_target(object_connect main object_connect_main)
add_test(NAME object_connect.main COMMAND ${object_connect_main} --frames 30)
set_tests_properties(object_connect.main PROPERTIES LABELS "gpu" TIMEOUT 120)
