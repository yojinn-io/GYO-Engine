# Object_FPS_PVP's own tests, carried over from master's tests/object_fps_pvp
# for the units the logic library carries (porting inventory, section 8), and
# the port's own (port declaration, section 3).
# Headless: no window, GPU, audio device or network socket.
gyo_get_game_target(object_fps_pvp logic object_fps_pvp_logic)
# The arena's map reader (data/ArenaMap) is in the world library, with the voxel_map Module it reads through.
gyo_get_game_target(object_fps_pvp world object_fps_pvp_world)
add_executable(gyo_object_fps_pvp_logic_tests
    TestMain.cpp
    AddressTextTests.cpp
    AdmissionTokenTests.cpp
    AnimationTests.cpp
    ArenaTests.cpp
    ControlsTests.cpp
    HitFeedbackTests.cpp
    PlanarMovementTests.cpp
    PlayerSaveTests.cpp
    SaveSyncTests.cpp
    RulesTests.cpp
    SoundCuesTests.cpp)
target_link_libraries(gyo_object_fps_pvp_logic_tests PRIVATE ${object_fps_pvp_logic} ${object_fps_pvp_world} doctest::doctest)
# The game's own content files the parsers are checked against.
target_compile_definitions(gyo_object_fps_pvp_logic_tests PRIVATE
    PVP_CONTENT_ROOT="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp"
    PVP_HIT_FEEDBACK_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/ui/hit_feedback.json"
    PVP_SOUND_SETTINGS_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/audio/sfx.json"
    PVP_ADMISSION_VECTORS="${GYO_REPOSITORY_ROOT}/games/object_fps_pvp/services/token/testdata/admission_vectors.txt")
if(MSVC)
    target_compile_options(gyo_object_fps_pvp_logic_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_object_fps_pvp_logic_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_fps_pvp_logic_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_fps_pvp.logic COMMAND gyo_object_fps_pvp_logic_tests)
set_tests_properties(object_fps_pvp.logic PROPERTIES LABELS "cpu" TIMEOUT 60)

# The game in a World: a lone Match the test steers, and a Match with Clients over
# the net Module's memory links (no socket), one fixed step a frame.
gyo_get_game_target(object_fps_pvp world object_fps_pvp_world)
add_executable(gyo_object_fps_pvp_world_tests world/TestMain.cpp world/WorldTests.cpp world/AdmissionTests.cpp
    world/ReplaceTests.cpp world/SavesTests.cpp)
target_include_directories(gyo_object_fps_pvp_world_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/world")
# SavesTests answers as the save service through the test library gyo_test_loopback_listener (tests/common/os).
target_link_libraries(gyo_object_fps_pvp_world_tests PRIVATE gyo_test_loopback_listener)
target_link_libraries(gyo_object_fps_pvp_world_tests PRIVATE ${object_fps_pvp_world} doctest::doctest)
gyo_product_units(gyo_object_fps_pvp_world_tests MODULES physics_3d)
target_compile_definitions(gyo_object_fps_pvp_world_tests PRIVATE
    PVP_CONTENT_ROOT="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp")
if(MSVC)
    target_compile_options(gyo_object_fps_pvp_world_tests PRIVATE /W4 /permissive- /utf-8 /bigobj)
    target_compile_definitions(gyo_object_fps_pvp_world_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_fps_pvp_world_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_fps_pvp.world COMMAND gyo_object_fps_pvp_world_tests "--test-case=world:*")
set_tests_properties(object_fps_pvp.world PROPERTIES LABELS "cpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

# The Client without a window or a GPU: its Game World and its Render World (no sdl_gpu; the picture is read at the
# Submit stage), a Match and a second Client over memory links, the content through the asset Module, the input
# and window Messages written by a test System.
gyo_get_game_target(object_fps_pvp client object_fps_pvp_client)
add_executable(gyo_object_fps_pvp_client_tests world/TestMain.cpp client/ClientTests.cpp)
target_include_directories(gyo_object_fps_pvp_client_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/world")
target_link_libraries(gyo_object_fps_pvp_client_tests PRIVATE ${object_fps_pvp_client} GYO::Threads GYO::RenderWorld doctest::doctest)
gyo_product_units(gyo_object_fps_pvp_client_tests MODULES physics_3d render_world world_render camera_render sprite_render mesh_render visibility
    world_ui ui_render)
target_compile_definitions(gyo_object_fps_pvp_client_tests PRIVATE
    PVP_CONTENT_ROOT="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp")
if(MSVC)
    target_compile_options(gyo_object_fps_pvp_client_tests PRIVATE /W4 /permissive- /utf-8 /bigobj)
    target_compile_definitions(gyo_object_fps_pvp_client_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_fps_pvp_client_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_fps_pvp.client COMMAND gyo_object_fps_pvp_client_tests "--test-case=client:*")
set_tests_properties(object_fps_pvp.client PROPERTIES LABELS "cpu" TIMEOUT 120
    FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

# The Client program: the window and the GPU device open, the content loads, 30 frames are drawn and shown, and it
# closes its window and exits with 0 -- the lobby (main), and the Client with --connect (no Match: it shows that it
# is connecting; main_connect). Label gpu, as object_connect.main.
gyo_get_game_target(object_fps_pvp main object_fps_pvp_main)
add_test(NAME object_fps_pvp.main COMMAND ${object_fps_pvp_main} --frames 30)
set_tests_properties(object_fps_pvp.main PROPERTIES LABELS "gpu" TIMEOUT 120)
add_test(NAME object_fps_pvp.main_connect COMMAND ${object_fps_pvp_main} --frames 30 --connect 127.0.0.1:40000)
set_tests_properties(object_fps_pvp.main_connect PROPERTIES LABELS "gpu" TIMEOUT 120)

# The Match program: it reads the staged content, listens on a UDP port and
# stops itself after 64 steps (one second). Label network: it opens a socket.
gyo_get_game_target(object_fps_pvp match object_fps_pvp_match)
add_test(NAME object_fps_pvp.match COMMAND ${object_fps_pvp_match} --port 0 --ticks 64)
set_tests_properties(object_fps_pvp.match PROPERTIES LABELS "network" TIMEOUT 60)

# The Match executable as the matches service starts it (gateway declaration, segment 2): its key on the standard
# input, listening on its port; a bad key refuses to start. Go on the host; label network (a real UDP port).
gyo_get_game_target(object_fps_pvp match object_fps_pvp_match_target)
add_test(NAME object_fps_pvp.match_process
    COMMAND "${CMAKE_COMMAND}" -E env GOWORK=off "GYO_PVP_MATCH=$<TARGET_FILE:${object_fps_pvp_match_target}>"
        "GYO_PVP_CONTENT=${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp"
        "${GYO_GO_EXECUTABLE}" test -count=1 -v -run "TestTheRealMatch" ./services/matches/
    WORKING_DIRECTORY "${GYO_REPOSITORY_ROOT}/games/object_fps_pvp")
set_tests_properties(object_fps_pvp.match_process PROPERTIES LABELS "network" TIMEOUT 120)
