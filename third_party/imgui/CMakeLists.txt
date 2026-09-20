include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.5
)

FetchContent_MakeAvailable(imgui)

add_library(imgui_core STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)

target_include_directories(imgui_core PUBLIC
    ${imgui_SOURCE_DIR}
)

add_library(imgui::core ALIAS imgui_core)

add_library(imgui_sdl3_sdlrenderer3 STATIC
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

target_include_directories(imgui_sdl3_sdlrenderer3 PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_sdl3_sdlrenderer3 PUBLIC
    imgui::core
    SDL3::SDL3
)

add_library(imgui::sdl3_sdlrenderer3 ALIAS imgui_sdl3_sdlrenderer3)
