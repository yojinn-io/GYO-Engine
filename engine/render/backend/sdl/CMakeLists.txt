add_library(gyo_render_backend_sdl STATIC
    src/SdlRenderer.cpp
)

add_library(GYO::RenderBackendSDL ALIAS gyo_render_backend_sdl)

target_include_directories(gyo_render_backend_sdl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
# The backend reads its own native header without linking its native target.
target_include_directories(gyo_render_backend_sdl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include
)

target_link_libraries(gyo_render_backend_sdl PUBLIC
    GYO::Engine
    GYO::PlatformSDL
)
target_link_libraries(gyo_render_backend_sdl PRIVATE GYO::PlatformSDLNative SDL3::SDL3)

target_compile_features(gyo_render_backend_sdl PUBLIC cxx_std_20)

# Linking this target declares direct use of SDL (docs/architecture/platform-sdl.md).
add_library(gyo_render_backend_sdl_native INTERFACE)
add_library(GYO::RenderBackendSDLNative ALIAS gyo_render_backend_sdl_native)
target_include_directories(gyo_render_backend_sdl_native INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include
)
target_link_libraries(gyo_render_backend_sdl_native INTERFACE
    GYO::RenderBackendSDL
    SDL3::SDL3
)
