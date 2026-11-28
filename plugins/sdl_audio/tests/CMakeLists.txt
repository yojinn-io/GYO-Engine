# Tests of the sdl_audio Plugin (docs/architecture/target/v2/sdl.zh-Hant.md,
# sdl_audio). They come and go with the Plugin's row (SDL-2) and include no SDL
# (SDL-1): SDL's audio driver is picked through the environment, where SDL reads
# its hints. The dummy and disk drivers need no audio device, so every CI row
# runs them headless.
add_executable(gyo_sdl_audio_tests TestMain.cpp SDLAudioPluginTests.cpp)
gyo_product_units(gyo_sdl_audio_tests PLUGINS sdl_audio)
# A failed SDL assertion ends the run, no dialog (plugins/sdl/tests/adapters/sdl/UnattendedAssertions.hpp).
target_link_libraries(gyo_sdl_audio_tests PRIVATE gyo_sdl_unattended_assertions)
target_link_libraries(gyo_sdl_audio_tests PRIVATE GYO::OS doctest::doctest)
if(MSVC)
    target_compile_options(gyo_sdl_audio_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_sdl_audio_tests PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_sdl_audio_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME sdl_audio.dummy COMMAND gyo_sdl_audio_tests "--test-case=dummy:*")
set_tests_properties(sdl_audio.dummy PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy")

# The disk driver writes what the device would play to a file. A timescale of
# 0.1 waits 1 ms per 480-frame block: fast, but bounded. The format hints pin the
# device to the mixer's format, so the file holds the mixer's samples unconverted.
set(gyo_sdl_audio_disk_file "${CMAKE_CURRENT_BINARY_DIR}/sdl_audio_disk.raw")
add_test(NAME sdl_audio.disk COMMAND gyo_sdl_audio_tests "--test-case=disk:*")
set_tests_properties(sdl_audio.disk PROPERTIES ENVIRONMENT
    "SDL_AUDIO_DRIVER=disk;SDL_AUDIO_DISK_TIMESCALE=0.1;SDL_AUDIO_DISK_OUTPUT_FILE=${gyo_sdl_audio_disk_file};SDL_AUDIO_FORMAT=S16;SDL_AUDIO_FREQUENCY=48000;SDL_AUDIO_CHANNELS=2;SDL_AUDIO_DEVICE_SAMPLE_FRAMES=480")

add_test(NAME sdl_audio.unavailable COMMAND gyo_sdl_audio_tests "--test-case=unavailable:*")
set_tests_properties(sdl_audio.unavailable PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=gyo-no-such-driver")

set_tests_properties(sdl_audio.dummy sdl_audio.disk sdl_audio.unavailable PROPERTIES
    LABELS "cpu" TIMEOUT 60 FAIL_REGULAR_EXPRESSION "test cases: +0 [|]")

# Not a CTest: records the default playback device's block sizes and callback
# gaps on a real machine, the input of the mixer's scheduleDelay
# (docs/architecture/target/v2/sdl.zh-Hant.md, sdl_audio). One JSON line.
add_executable(gyo_sdl_audio_block_record BlockRecord.cpp)
gyo_product_units(gyo_sdl_audio_block_record PLUGINS sdl_audio)
