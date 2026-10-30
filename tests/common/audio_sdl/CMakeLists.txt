# The SDL audio backend, driven only through its neutral API: the tests include
# and call no SDL, and pick SDL's driver through the environment (SDL reads
# SDL_AUDIO_DRIVER and its other hints from there). The dummy and disk drivers
# need no audio device, so CI runs these headless.
add_executable(gyo_audio_sdl_tests SdlAudioOutputTests.cpp)
target_link_libraries(gyo_audio_sdl_tests PRIVATE GYO::AudioBackendSDL gyo_test_support doctest::doctest)
target_compile_features(gyo_audio_sdl_tests PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(gyo_audio_sdl_tests)

add_test(NAME audio_sdl.dummy COMMAND gyo_audio_sdl_tests "-tc=*dummy driver*")
set_tests_properties(audio_sdl.dummy PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=dummy" LABELS "cpu" TIMEOUT 60)

# The disk driver writes what the device would play to a file; a timescale of 0
# writes as fast as it can. The format hints pin the device to the mixer's format
# so the file holds the mixer's samples unconverted.
set(gyo_audio_disk_file "${CMAKE_CURRENT_BINARY_DIR}/audio_sdl_disk.raw")
add_test(NAME audio_sdl.disk COMMAND gyo_audio_sdl_tests "-tc=*disk driver*")
set_tests_properties(audio_sdl.disk PROPERTIES
    ENVIRONMENT "SDL_AUDIO_DRIVER=disk;SDL_AUDIO_DISK_TIMESCALE=0;SDL_AUDIO_DISK_OUTPUT_FILE=${gyo_audio_disk_file};SDL_AUDIO_FORMAT=S16;SDL_AUDIO_FREQUENCY=48000;SDL_AUDIO_CHANNELS=2;SDL_AUDIO_DEVICE_SAMPLE_FRAMES=480"
    LABELS "cpu" TIMEOUT 60)

add_test(NAME audio_sdl.unavailable COMMAND gyo_audio_sdl_tests "-tc=*unavailable*")
set_tests_properties(audio_sdl.unavailable PROPERTIES ENVIRONMENT "SDL_AUDIO_DRIVER=gyo-no-such-driver" LABELS "cpu" TIMEOUT 60)

# Not a CTest: records the default device's block sizes and callback gaps on a
# real machine (v8-engine-audio-delay), printing one line.
add_executable(gyo_audio_block_record BlockRecord.cpp)
target_link_libraries(gyo_audio_block_record PRIVATE GYO::AudioBackendSDL)
target_compile_features(gyo_audio_block_record PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(gyo_audio_block_record)

# Not a CTest: the Engine L2 of v8-engine-audio-record (record only), run by
# onset_record.py, which sets SDL's driver through the environment.
add_executable(gyo_audio_onset_record OnsetRecord.cpp)
target_link_libraries(gyo_audio_onset_record PRIVATE GYO::AudioBackendSDL)
target_compile_features(gyo_audio_onset_record PRIVATE cxx_std_20)
gyo_copy_runtime_dlls(gyo_audio_onset_record)
