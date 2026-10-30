# Plays a GYO::Audio mixer on the default playback device through an SDL audio
# stream. SDL stays inside this target: it links SDL3 PRIVATE and offers no
# native target, so no user of audio reaches SDL (docs/architecture/audio.md).
add_library(gyo_audio_sdl STATIC
    src/SdlAudioOutput.cpp
)
add_library(GYO::AudioBackendSDL ALIAS gyo_audio_sdl)

target_include_directories(gyo_audio_sdl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_audio_sdl PUBLIC cxx_std_20)
target_link_libraries(gyo_audio_sdl PUBLIC GYO::Audio PRIVATE SDL3::SDL3)
