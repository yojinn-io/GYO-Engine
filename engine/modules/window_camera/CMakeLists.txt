# The glue between window and camera (docs/architecture/target/v2/window.zh-Hant.md, "window_camera"): writes the
# window's size into camera's CameraTargetSizes as the main target.
add_library(gyo_window_camera STATIC
    src/WindowCameraModule.cpp
)
add_library(GYO::WindowCamera ALIAS gyo_window_camera)

target_include_directories(gyo_window_camera PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; window and camera come from config/modules.csv (O-6).
target_link_libraries(gyo_window_camera PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(window_camera)
target_compile_features(gyo_window_camera PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_window_camera PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_window_camera PRIVATE -Wall -Wextra -Wpedantic)
endif()
