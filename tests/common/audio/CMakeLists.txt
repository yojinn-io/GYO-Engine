# The mixer's sample-domain core is tested offline, without a device: the test
# reaches the internal VoiceMixer header (engine/audio/src) on purpose, since
# the public Mixer that wraps it arrives with v8-engine-audio-schedule.
add_executable(gyo_audio_tests MixerTests.cpp)
target_include_directories(gyo_audio_tests PRIVATE ${PROJECT_SOURCE_DIR}/engine/audio/src)
target_link_libraries(gyo_audio_tests PRIVATE GYO::Audio gyo_test_support doctest::doctest)
add_test(NAME gyo_audio_tests COMMAND gyo_audio_tests)
set_tests_properties(gyo_audio_tests PROPERTIES LABELS "cpu" TIMEOUT 60)

# The mixer is a leaf above the time base: its links, private and interface
# alike, are exactly GYO::Base and GYO::Time, so no SDL, asset or engine library
# edge appears (docs/architecture/audio.md, Design Invariants 1 and 2;
# gyo_expect_links is defined with the time tests, which configure first).
# Checked once the whole project is configured.
function(gyo_expect_audio_links)
    gyo_expect_links(gyo_audio GYO::Base GYO::Time)
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL gyo_expect_audio_links)
