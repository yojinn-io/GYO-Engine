# The engine audio mixer: shared immutable PCM16 clips, voices started at sample
# positions and an integer mix to 48 kHz stereo PCM16, and the loader of the
# "sound" type (WAV). Above GYO::Time and GYO::Asset (the loader interface, from
# config/modules.csv); no SDL and no product knowledge, so it is tested offline
# without a device.
add_library(gyo_audio STATIC
    src/Clip.cpp
    src/Mixer.cpp
    src/VoiceMixer.cpp
    src/WavLoader.cpp
)
add_library(GYO::Audio ALIAS gyo_audio)

target_include_directories(gyo_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_audio PUBLIC cxx_std_20)
# Links to core, ecs and orchestration are written here; links to other Modules
# come from config/modules.csv (O-6).
target_link_libraries(gyo_audio PUBLIC GYO::Base GYO::Time)
gyo_link_registered_dependencies(audio)
