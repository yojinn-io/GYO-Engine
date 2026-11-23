# The sdl_audio Plugin (config/plugins.csv): the audio output on SDL. It creates
# the mixer (or plays the product's), registers the sound loader, opens the
# default playback device and stops it in OnDestroy
# (docs/architecture/target/v2/sdl.zh-Hant.md, sdl_audio).
#
# SDL is linked PRIVATE and only src/adapters/sdl/ includes SDL3/ (Design
# Invariant SDL-1); the public headers speak GYO's words.
add_library(gyo_sdl_audio STATIC
    src/AudioOutput.cpp
    src/SDLAudioPlugin.cpp
    src/adapters/sdl/AudioDevice.cpp
)
add_library(GYO::SDLAudio ALIAS gyo_sdl_audio)
target_include_directories(gyo_sdl_audio
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(gyo_sdl_audio PUBLIC cxx_std_20)
target_link_libraries(gyo_sdl_audio PUBLIC GYO::Orchestration PRIVATE SDL3::SDL3-static)
gyo_link_registered_dependencies(sdl_audio)
if(MSVC)
    target_compile_options(gyo_sdl_audio PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_sdl_audio PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
