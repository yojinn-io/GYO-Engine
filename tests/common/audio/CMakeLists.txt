# The mixer's sample-domain core is tested offline, without a device: the test
# reaches the internal VoiceMixer header (engine/audio/src) on purpose, since
# the public Mixer that wraps it arrives with v8-engine-audio-schedule.
add_executable(gyo_audio_tests MixerTests.cpp ScheduleTests.cpp CharacterizationTests.cpp StopTests.cpp ClipSlotTests.cpp PlayheadTests.cpp)
target_include_directories(gyo_audio_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/audio/src)
target_link_libraries(gyo_audio_tests PRIVATE GYO::Audio gyo_test_support doctest::doctest)
# The ThreadSanitizer stress case runs as its own CTest, so its time never eats
# into the other cases' timeout. Its three runs are bounded inside the case at
# 20 s each, 60 s under ThreadSanitizer, so a run that never reaches its counts
# fails before this timeout.
set(GYO_AUDIO_STRESS_CASE "*under a polling thread*")
add_test(NAME gyo_audio_tests COMMAND gyo_audio_tests "--test-case-exclude=${GYO_AUDIO_STRESS_CASE}")
set_tests_properties(gyo_audio_tests PROPERTIES LABELS "cpu" TIMEOUT 60)
add_test(NAME gyo_audio_playhead_stress COMMAND gyo_audio_tests "--test-case=${GYO_AUDIO_STRESS_CASE}")
if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=thread")
    set(GYO_AUDIO_STRESS_TIMEOUT 200)
else()
    set(GYO_AUDIO_STRESS_TIMEOUT 90)
endif()
set_tests_properties(gyo_audio_playhead_stress PROPERTIES LABELS "cpu" TIMEOUT ${GYO_AUDIO_STRESS_TIMEOUT})

# Design Invariant 4: only Mixer::PlayheadOf reads the playhead board.
add_test(NAME gyo_audio_board_readers
         COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR}/engine/audio/src
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckBoardReaders.cmake)
set_tests_properties(gyo_audio_board_readers PROPERTIES LABELS "cpu" TIMEOUT 60)

# Replaces the global allocation functions, so it is its own executable.
add_executable(gyo_audio_alloc_tests RenderAllocTests.cpp)
target_link_libraries(gyo_audio_alloc_tests PRIVATE GYO::Audio doctest::doctest)
add_test(NAME gyo_audio_alloc_tests COMMAND gyo_audio_alloc_tests)
set_tests_properties(gyo_audio_alloc_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# The mixer is a leaf above the time base: its links, private and interface
# alike, are exactly GYO::Base and GYO::Time, so no SDL, asset or engine library
# edge appears (docs/architecture/audio.md, Design Invariants 1 and 2;
# gyo_expect_links is defined with the time tests, which configure first).
# Checked once the whole project is configured.
function(gyo_expect_audio_links)
    gyo_expect_links(gyo_audio GYO::Base GYO::Time)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_audio_links)
