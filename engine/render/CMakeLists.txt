add_library(gyo_render STATIC
    src/ColorTransform.cpp
    src/PrimitiveMesh.cpp
    src/RenderQueue.cpp
    src/Renderer.cpp
    src/ShaderLibrary.cpp
)

add_library(GYO::Render ALIAS gyo_render)

target_include_directories(gyo_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_render PUBLIC
    GYO::Engine
)
target_link_libraries(gyo_render PRIVATE nlohmann_json::nlohmann_json)

target_compile_features(gyo_render PUBLIC cxx_std_20)

if(GYO_ENABLE_SDL_RENDERER)
    add_subdirectory(backend/sdl)
endif()

if(GYO_ENABLE_SDL_GPU)
    include("${GYO_REPOSITORY_ROOT}/build/cmake/GyoShaders.cmake")
    gyo_add_shader_bundle(gyo_builtin_shaders
        SPEC "${GYO_REPOSITORY_ROOT}/engine/render/shaders/builtin/bundle.json"
        OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/shaders/builtin")
    set(gyo_builtin_shaders_DIRECTORY "${gyo_builtin_shaders_DIRECTORY}" PARENT_SCOPE)
    set(gyo_builtin_shaders_MANIFEST "${gyo_builtin_shaders_MANIFEST}" PARENT_SCOPE)
    add_subdirectory(backend/sdl_gpu)
endif()



# Narrow model/render bridge; neither core module depends on the other.
add_subdirectory(model)
