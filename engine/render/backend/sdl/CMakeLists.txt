add_library(gyo_render_backend_sdl STATIC
    src/SdlRenderer.cpp
)

add_library(GYO::RenderBackendSDL ALIAS gyo_render_backend_sdl)

target_include_directories(gyo_render_backend_sdl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_render_backend_sdl PUBLIC
    GYO::Engine
    GYO::PlatformSDL
)

target_compile_features(gyo_render_backend_sdl PUBLIC cxx_std_20)
