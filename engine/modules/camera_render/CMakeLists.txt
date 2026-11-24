# camera_render (docs/architecture/target/v2/render.zh-Hant.md, section 7): extracts the source World's cameras into
# the render World each frame.
add_library(gyo_camera_render STATIC
    src/CameraRenderModule.cpp
)
add_library(GYO::CameraRender ALIAS gyo_camera_render)

target_include_directories(gyo_camera_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Links to core, ecs and orchestration are written here; links to other Modules come from config/modules.csv (O-6).
target_link_libraries(gyo_camera_render PUBLIC GYO::Base GYO::Kernel GYO::Orchestration)
gyo_link_registered_dependencies(camera_render)
target_compile_features(gyo_camera_render PUBLIC cxx_std_20)
if(MSVC)
    target_compile_options(gyo_camera_render PRIVATE /W4 /permissive-)
else()
    target_compile_options(gyo_camera_render PRIVATE -Wall -Wextra -Wpedantic)
endif()
