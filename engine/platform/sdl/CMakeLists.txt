add_library(gyo_platform_sdl STATIC
    src/SdlPlatform.cpp
)

add_library(GYO::PlatformSDL ALIAS gyo_platform_sdl)

target_include_directories(gyo_platform_sdl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_platform_sdl PUBLIC
    GYO::Engine
    SDL3::SDL3
)

target_compile_features(gyo_platform_sdl PUBLIC cxx_std_20)
