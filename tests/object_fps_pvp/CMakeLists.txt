# Object_FPS_PVP's own tests, carried over from master's tests/object_fps_pvp
# for the units the logic library carries (porting inventory, section 8), and
# the port's own (port declaration, section 3).
# Headless: no window, GPU, audio device or network socket.
gyo_get_game_target(object_fps_pvp logic object_fps_pvp_logic)
# The arena's map reader (data/ArenaMap) is in the world library, with the voxel_map Module it reads through.
gyo_get_game_target(object_fps_pvp world object_fps_pvp_world)
add_executable(gyo_object_fps_pvp_logic_tests
    TestMain.cpp
    AdmissionTokenTests.cpp
    AnimationTests.cpp
    ArenaTests.cpp
    ControlsTests.cpp
    HitFeedbackTests.cpp
    InterpolationTests.cpp
    LoadThreadsTests.cpp
    PlanarMovementTests.cpp
    PlayerRosterTests.cpp
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
    PVP_ADMISSION_VECTORS="${GYO_REPOSITORY_ROOT}/games/object_fps_pvp/services/token/testdata/admission_vectors.txt"
    PVP_ROSTER_VECTOR="${GYO_REPOSITORY_ROOT}/games/object_fps_pvp/services/matches/testdata/roster_line.txt")
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
    world/ReplaceTests.cpp world/SavesTests.cpp world/LocalTests.cpp world/MapLoadTests.cpp world/ModeTests.cpp
    world/GameplaySetTests.cpp world/NameTests.cpp world/InteractionTests.cpp world/SceneTests.cpp)
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

# The core PvP script (mode-config declaration, section 5): the game's three ends play a fixed sequence of inputs and
# print every World's authority digest at every frame. The A/B of a change runs the build before it and the build
# after it on one machine and compares the lines; here it runs with --check: it went through every path it is for.
add_executable(gyo_object_fps_pvp_core_script script/CorePvpScript.cpp)
target_link_libraries(gyo_object_fps_pvp_core_script PRIVATE ${object_fps_pvp_world})
gyo_product_units(gyo_object_fps_pvp_core_script MODULES physics_3d)
target_compile_definitions(gyo_object_fps_pvp_core_script PRIVATE PVP_CONTENT_ROOT="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp")
if(MSVC)
    target_compile_options(gyo_object_fps_pvp_core_script PRIVATE /W4 /permissive- /utf-8 /bigobj)
    target_compile_definitions(gyo_object_fps_pvp_core_script PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_fps_pvp_core_script PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_fps_pvp.core_script COMMAND gyo_object_fps_pvp_core_script --check)
set_tests_properties(object_fps_pvp.core_script PROPERTIES LABELS "cpu" TIMEOUT 120)

# The Client without a window or a GPU: its Game World and its Render World (no sdl_gpu; the picture is read at the
# Submit stage), a Match and a second Client over memory links, the content through the asset Module, the input
# and window Messages written by a test System.
gyo_get_game_target(object_fps_pvp client object_fps_pvp_client)
add_executable(gyo_object_fps_pvp_client_tests world/TestMain.cpp client/ClientTests.cpp)
target_include_directories(gyo_object_fps_pvp_client_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/world")
# The Gateway's tests answer as the Gateway through gyo_test_loopback_listener (tests/common/os).
target_link_libraries(gyo_object_fps_pvp_client_tests PRIVATE ${object_fps_pvp_client} GYO::Threads GYO::RenderWorld gyo_test_loopback_listener
    doctest::doctest)
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
# Four ctest entries that run in parallel, each a range of the "client:" cases in their order in the file (doctest's
# --first/--last; the last range is open, so a case added at the end is run): the cases wait for the maps the asset
# threads read (Setup::Loaded), and one process took up to 118 s on the macOS runner (2026-11-26, big map #364). The
# ranges are cut by the cases' time (macOS, 2026-11-27: about 13 s, 14 s, 12 s and 8 s); cut them again when one entry
# takes about twice another (doctest --duration=true gives each case's time).
set(object_fps_pvp_client_ranges "1:6" "7:14" "15:30" "31:")
set(object_fps_pvp_client_part 0)
foreach(range IN LISTS object_fps_pvp_client_ranges)
    math(EXPR object_fps_pvp_client_part "${object_fps_pvp_client_part} + 1")
    string(REPLACE ":" ";" bounds "${range}")
    list(GET bounds 0 first)
    list(GET bounds 1 last)
    set(arguments "--test-case=client:*" "--first=${first}")
    if(NOT last STREQUAL "")
        list(APPEND arguments "--last=${last}")
    endif()
    add_test(NAME object_fps_pvp.client.${object_fps_pvp_client_part} COMMAND gyo_object_fps_pvp_client_tests ${arguments})
    set_tests_properties(object_fps_pvp.client.${object_fps_pvp_client_part} PROPERTIES LABELS "cpu" TIMEOUT 300
        FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")
endforeach()

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
# --load-threads outside 1 to 8 (big-map declaration 17.8): the Match still plays, on the machine's choice, and says
# that it ignored the count.
add_test(NAME object_fps_pvp.match_load_threads COMMAND ${object_fps_pvp_match} --port 0 --ticks 1 --load-threads 100)
set_tests_properties(object_fps_pvp.match_load_threads PROPERTIES LABELS "network" TIMEOUT 60
    PASS_REGULAR_EXPRESSION "--load-threads 100 is outside 1 to 8: the map loads on [2-8] threads")

# --mode names a mode that is not installed (mode-config declaration 3.5): the Match does not start, and says so.
add_test(NAME object_fps_pvp.match_mode_unknown COMMAND ${object_fps_pvp_match} --port 0 --ticks 1 --mode no_such_mode)
set_tests_properties(object_fps_pvp.match_mode_unknown PROPERTIES LABELS "cpu" TIMEOUT 60
    PASS_REGULAR_EXPRESSION "no installed mode has the id 'no_such_mode'")

# The Match executable as the matches service starts it (gateway declaration, segment 2): its key on the standard
# input, listening on its port; a bad key refuses to start. Go on the host; label network (a real UDP port).
gyo_get_game_target(object_fps_pvp match object_fps_pvp_match_target)
add_test(NAME object_fps_pvp.match_process
    COMMAND "${CMAKE_COMMAND}" -E env GOWORK=off "GYO_PVP_MATCH=$<TARGET_FILE:${object_fps_pvp_match_target}>"
        "GYO_PVP_CONTENT=${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp"
        "${GYO_GO_EXECUTABLE}" test -count=1 -v -run "TestTheRealMatch" ./services/matches/
    WORKING_DIRECTORY "${GYO_REPOSITORY_ROOT}/games/object_fps_pvp")
set_tests_properties(object_fps_pvp.match_process PROPERTIES LABELS "network" TIMEOUT 120)

# The whole line (game-flow declaration, segment 3): every service in one Go test process, real Matches, and the
# Client test program's "network:" case through the Gateway over UDP; and its "network-room:" case (two Clients in one
# room). Go on the host; label network.
gyo_get_game_target(object_fps_pvp match object_fps_pvp_match_for_client)
add_test(NAME object_fps_pvp.gateway_client
    COMMAND "${CMAKE_COMMAND}" -E env GOWORK=off "GYO_PVP_MATCH=$<TARGET_FILE:${object_fps_pvp_match_for_client}>"
        "GYO_PVP_CONTENT=${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp"
        "GYO_PVP_CLIENT_TESTS=$<TARGET_FILE:gyo_object_fps_pvp_client_tests>"
        "${GYO_GO_EXECUTABLE}" test -count=1 -v -run "TestTheRealClientPlaysThroughTheGateway|TestTwoClientsInARoom" ./services/stack/
    WORKING_DIRECTORY "${GYO_REPOSITORY_ROOT}/games/object_fps_pvp")
set_tests_properties(object_fps_pvp.gateway_client PROPERTIES LABELS "network" TIMEOUT 240)
# The retired names (v8-arch-gyo2-interact-door step 0): the game-flow Scene was renamed Screen, and this check keeps
# the old identifiers and file names from coming back. -B: no bytecode is written into the source tree.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME object_fps_pvp.retired_names
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/naming/retired_names.py" --root "${GYO_REPOSITORY_ROOT}")
add_test(NAME object_fps_pvp.retired_names_selftest
    COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/naming" -p "test_retired_names.py" -v)
set_tests_properties(object_fps_pvp.retired_names object_fps_pvp.retired_names_selftest PROPERTIES LABELS "cpu" TIMEOUT 60)
