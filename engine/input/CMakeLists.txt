add_library(gyo_input STATIC
    src/InputActionMap.cpp
)

add_library(GYO::Input ALIAS gyo_input)

target_include_directories(gyo_input PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(gyo_input PUBLIC cxx_std_20)

if(GYO_ENABLE_SDL_INPUT)
    add_subdirectory(backend/sdl)
endif()


