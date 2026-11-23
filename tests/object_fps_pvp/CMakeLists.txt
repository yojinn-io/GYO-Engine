# Object_FPS_PVP's own tests, carried over from master's tests/object_fps_pvp
# for the units the logic library carries (porting inventory, section 8).
# Headless: no window, GPU, audio device or network.
gyo_get_game_target(object_fps_pvp logic object_fps_pvp_logic)
add_executable(gyo_object_fps_pvp_logic_tests
    TestMain.cpp
    AddressTextTests.cpp
    HitFeedbackTests.cpp
    PlanarMovementTests.cpp
    SoundCuesTests.cpp)
target_link_libraries(gyo_object_fps_pvp_logic_tests PRIVATE ${object_fps_pvp_logic} doctest::doctest)
# The game's own content files the parsers are checked against.
target_compile_definitions(gyo_object_fps_pvp_logic_tests PRIVATE
    PVP_HIT_FEEDBACK_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/ui/hit_feedback.json"
    PVP_SOUND_SETTINGS_PATH="${GYO_REPOSITORY_ROOT}/assets/object_fps_pvp/audio/sfx.json")
if(MSVC)
    target_compile_options(gyo_object_fps_pvp_logic_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(gyo_object_fps_pvp_logic_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_object_fps_pvp_logic_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME object_fps_pvp.logic COMMAND gyo_object_fps_pvp_logic_tests)
set_tests_properties(object_fps_pvp.logic PROPERTIES LABELS "cpu" TIMEOUT 60)
