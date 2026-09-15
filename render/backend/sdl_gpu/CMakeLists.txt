set(GYO_SDL_GPU_SHADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.vert.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.frag.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_post.vert.hlsl"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_post.frag.hlsl"
)

# The standalone HLSL files are the only shader sources. Generate a private
# build-tree header so every static-library consumer gets the same shaders
# without runtime file deployment or source-tree paths in the backend API.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${GYO_SDL_GPU_SHADER_FILES}
)
file(READ
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.vert.hlsl"
    GYO_SDL_GPU_UNLIT_VERTEX_SOURCE
)
file(READ
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/unlit.frag.hlsl"
    GYO_SDL_GPU_UNLIT_FRAGMENT_SOURCE
)
file(READ
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_post.vert.hlsl"
    GYO_SDL_GPU_SCENE_POST_VERTEX_SOURCE
)
file(READ
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/scene_post.frag.hlsl"
    GYO_SDL_GPU_SCENE_POST_FRAGMENT_SOURCE
)
set(GYO_SDL_GPU_GENERATED_INCLUDE_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
set(GYO_SDL_GPU_EMBEDDED_SHADER_HEADER
    "${GYO_SDL_GPU_GENERATED_INCLUDE_DIRECTORY}/SdlGpuEmbeddedShaders.hpp"
)
file(MAKE_DIRECTORY "${GYO_SDL_GPU_GENERATED_INCLUDE_DIRECTORY}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/SdlGpuEmbeddedShaders.hpp.in"
    "${GYO_SDL_GPU_EMBEDDED_SHADER_HEADER}"
    @ONLY
)

add_library(gyo_render_backend_sdl_gpu STATIC
    src/SdlGpuRenderDevice.cpp
    "${GYO_SDL_GPU_EMBEDDED_SHADER_HEADER}"
    ${GYO_SDL_GPU_SHADER_FILES}
)

# HLSL is compiled by D3DCompile when the render device is initialized. Keep
# the files visible in IDEs without asking Visual Studio to compile them as
# build-time shader targets.
set_source_files_properties(${GYO_SDL_GPU_SHADER_FILES} PROPERTIES
    HEADER_FILE_ONLY TRUE
)
source_group("Shaders" FILES ${GYO_SDL_GPU_SHADER_FILES})

add_library(GYO::RenderBackendSDLGPU ALIAS gyo_render_backend_sdl_gpu)

target_include_directories(gyo_render_backend_sdl_gpu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(gyo_render_backend_sdl_gpu PRIVATE
    "${GYO_SDL_GPU_GENERATED_INCLUDE_DIRECTORY}"
)

target_link_libraries(gyo_render_backend_sdl_gpu
    PUBLIC
        GYO::Render
        GYO::PlatformSDL
    PRIVATE
        d3dcompiler
)

target_compile_definitions(gyo_render_backend_sdl_gpu PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_features(gyo_render_backend_sdl_gpu PUBLIC cxx_std_20)
