add_library(gyo_platform_sdl STATIC
    src/SdlPlatform.cpp
)

add_library(GYO::PlatformSDL ALIAS gyo_platform_sdl)

target_include_directories(gyo_platform_sdl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# The backend reads its own native header without linking its native target.
target_include_directories(gyo_platform_sdl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include
)

target_link_libraries(gyo_platform_sdl PUBLIC
    GYO::Engine
    SDL3::SDL3
)

target_compile_features(gyo_platform_sdl PUBLIC cxx_std_20)

# Linking this target declares direct use of SDL (docs/architecture/platform-sdl.md).
add_library(gyo_platform_sdl_native INTERFACE)
add_library(GYO::PlatformSDLNative ALIAS gyo_platform_sdl_native)
target_include_directories(gyo_platform_sdl_native INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include
)
target_link_libraries(gyo_platform_sdl_native INTERFACE
    GYO::PlatformSDL
    SDL3::SDL3
)
