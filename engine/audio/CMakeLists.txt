# The engine audio mixer: immutable PCM16 clips, voices started at sample
# positions and an integer mix to 48 kHz stereo PCM16. Above GYO::Time; no SDL
# and no asset or product knowledge, so it is tested offline without a device.
add_library(gyo_audio STATIC
    src/ClipBank.cpp
    src/Mixer.cpp
    src/VoiceMixer.cpp
)
add_library(GYO::Audio ALIAS gyo_audio)

target_include_directories(gyo_audio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_audio PUBLIC cxx_std_20)
target_link_libraries(gyo_audio PUBLIC GYO::Base GYO::Time)
