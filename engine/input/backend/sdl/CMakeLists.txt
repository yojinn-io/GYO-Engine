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
target_link_libraries(gyo_input_sdl PRIVATE GYO::PlatformSDLNative)

target_compile_features(gyo_input_sdl PUBLIC cxx_std_20)

# Linking this target declares direct use of SDL (docs/architecture/platform-sdl.md).
add_library(gyo_input_sdl_native INTERFACE)
add_library(GYO::InputBackendSDLNative ALIAS gyo_input_sdl_native)
target_include_directories(gyo_input_sdl_native INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include
)
target_link_libraries(gyo_input_sdl_native INTERFACE
    GYO::InputBackendSDL
    GYO::PlatformSDLNative
    SDL3::SDL3
)
