# sdl_window (config/plugins.csv; docs/architecture/target/v2/sdl.zh-Hant.md, "sdl_window"): the window, the OS
# events as input's and window's Messages, the game's requests and the window's runner, through SDL. SDL3 is linked
# PRIVATE and only src/adapters/sdl/ and tests/adapters/sdl/ include SDL3/ (Design Invariant SDL-1).
add_library(gyo_sdl_window STATIC
    src/SDLWindowPlugin.cpp
    src/adapters/sdl/WindowBackend.cpp
)
add_library(GYO::SDLWindow ALIAS gyo_sdl_window)

target_include_directories(gyo_sdl_window PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# The Modules it writes and GYO::SDL come from config/plugins.csv (O-6).
target_link_libraries(gyo_sdl_window PUBLIC GYO::Base GYO::Kernel GYO::Orchestration GYO::Math GYO::Time
                      PRIVATE GYO::OS SDL3::SDL3-static)
gyo_link_registered_dependencies(sdl_window)
target_compile_features(gyo_sdl_window PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_sdl_window PRIVATE /W4 /permissive-)
    target_compile_definitions(gyo_sdl_window PRIVATE NOMINMAX)
else()
    target_compile_options(gyo_sdl_window PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
