add_library(gyo_text INTERFACE)
add_library(GYO::Text ALIAS gyo_text)

target_include_directories(gyo_text INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(gyo_text INTERFACE
    GYO::Engine
)

target_compile_features(gyo_text INTERFACE cxx_std_20)

if(GYO_BUILD_SDL_TTF_ADAPTER OR GYO_BUILD_OBJECT_FPS OR GYO_BUILD_UI_EDITOR)
    if(NOT TARGET SDL3_ttf::SDL3_ttf)
        message(FATAL_ERROR
            "The SDL_ttf text adapter requires the SDL3_ttf::SDL3_ttf target")
    endif()
    add_subdirectory(backend/sdl_ttf)
endif()
