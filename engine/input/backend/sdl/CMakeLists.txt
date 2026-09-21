add_library(gyo_input_sdl STATIC
    src/SdlInput.cpp
)

add_library(GYO::InputBackendSDL ALIAS gyo_input_sdl)

target_include_directories(gyo_input_sdl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_input_sdl PUBLIC
    GYO::Input
    GYO::PlatformSDL
    SDL3::SDL3
)

target_compile_features(gyo_input_sdl PUBLIC cxx_std_20)
