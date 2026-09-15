add_library(gyo_render STATIC
    src/ColorTransform.cpp
    src/PrimitiveMesh.cpp
    src/RenderQueue.cpp
)

add_library(GYO::Render ALIAS gyo_render)

target_include_directories(gyo_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_render PUBLIC
    GYO::Engine
)

target_compile_features(gyo_render PUBLIC cxx_std_20)

if(GYO_BUILD_RUNTIME OR GYO_BUILD_SANDBOX OR GYO_BUILD_UI_EDITOR)
    add_subdirectory(backend/sdl)
endif()

if(GYO_BUILD_SDL_GPU_BACKEND OR GYO_BUILD_OBJECT_FPS)
    if(NOT WIN32 OR NOT MSVC)
        message(FATAL_ERROR
            "GYO_BUILD_SDL_GPU_BACKEND currently requires Windows and MSVC")
    endif()
    add_subdirectory(backend/sdl_gpu)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
